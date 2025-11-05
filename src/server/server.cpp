#include "server.h"

#include "../storage/store.h"
#include "../server/connection.h" // connection.h should be in src/server
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <iostream>

namespace kvstore {

Server::Server(int port)
    : port_(port), store_(std::make_shared<Store>()) {
}

Server::~Server() {
    stop();
}

bool Server::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        std::cerr << "fcntl F_GETFL error: " << strerror(errno) << std::endl;
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cerr << "fcntl F_SETFL error: " << strerror(errno) << std::endl;
        return false;
    }

    return true;
}

bool Server::createListenSocket() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        std::cerr << "socket error: " << strerror(errno) << std::endl;
        return false;
    }

    int opt = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "setsockopt error: " << strerror(errno) << std::endl;
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    if (!setNonBlocking(listen_fd_)) {
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind error: " << strerror(errno) << std::endl;
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    if (listen(listen_fd_, SOMAXCONN) < 0) {
        std::cerr << "listen error: " << strerror(errno) << std::endl;
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    std::cout << "Server listening on port " << port_ << std::endl;
    return true;
}

void Server::acceptConnection() {
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(listen_fd_,
                              reinterpret_cast<sockaddr*>(&client_addr),
                              &client_len);

        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No more incoming connections right now (non-blocking)
                break;
            } else {
                std::cerr << "accept error: " << strerror(errno) << std::endl;
                break;
            }
        }

        if (!setNonBlocking(client_fd)) {
            close(client_fd);
            continue;
        }

        epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET; // edge-triggered read
        ev.data.fd = client_fd;

        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
            std::cerr << "epoll_ctl ADD client error: " << strerror(errno) << std::endl;
            close(client_fd);
            continue;
        }

        connections_[client_fd] = std::make_unique<Connection>(client_fd, store_);

        std::cout << "New connection: fd=" << client_fd
                  << ", total connections: " << connections_.size() << std::endl;
    }
}

void Server::handleClient(int fd, uint32_t events) {
    auto it = connections_.find(fd);
    if (it == connections_.end()) {
        return;
    }

    Connection* conn = it->second.get();
    bool keep_alive = true;

    if (events & EPOLLIN) {
        keep_alive = conn->handleRead();
    }

    // If EPOLLOUT is signaled (or we still have data), try to write
    if (keep_alive && (events & EPOLLOUT || conn->hasDataToWrite())) {
        keep_alive = conn->handleWrite();
    }

    if (events & (EPOLLERR | EPOLLHUP)) {
        keep_alive = false;
    }

    if (!keep_alive) {
        closeConnection(fd);
    }
}

void Server::closeConnection(int fd) {
    std::cout << "Closing connection: fd=" << fd << std::endl;

    if (epoll_fd_ >= 0) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    }

    // unique_ptr destructor will close socket via Connection::~Connection
    connections_.erase(fd);
}

void Server::run() {
    if (!createListenSocket()) {
        return;
    }

    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        std::cerr << "epoll_create1 error: " << strerror(errno) << std::endl;
        return;
    }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd_;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, listen_fd_, &ev) < 0) {
        std::cerr << "epoll_ctl ADD listen_fd error: " << strerror(errno) << std::endl;
        return;
    }

    running_ = true;

    const int MAX_EVENTS = 64;
    std::vector<epoll_event> events(MAX_EVENTS);

    std::cout << "Server running..." << std::endl;

    while (running_) {
        int nfds = epoll_wait(epoll_fd_, events.data(), MAX_EVENTS, 1000);

        if (nfds < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "epoll_wait error: " << strerror(errno) << std::endl;
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;

            if (fd == listen_fd_) {
                acceptConnection();
            } else {
                handleClient(fd, events[i].events);
            }
        }
    }

    // cleanup on exit
    stop();
}

void Server::stop() {
    running_ = false;

    // Remove and destroy all connections (Connection destructor closes fd)
    connections_.clear();

    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }

    if (listen_fd_ >= 0) {
        close(listen_fd_);
        listen_fd_ = -1;
    }

    std::cout << "Server stopped" << std::endl;
}

} // namespace kvstore
