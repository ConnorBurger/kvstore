#ifndef KVSTORE_SERVER_H
#define KVSTORE_SERVER_H

#include <atomic>
#include <memory>
#include <map>
#include <sys/epoll.h>

namespace kvstore {

    class Store;
    class Connection;

    class Server {
    public:
        explicit Server(int port);
        ~Server();

        // non-copyable
        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;

        bool setNonBlocking(int fd);
        bool createListenSocket();

        void run();
        void stop();

    private:
        void acceptConnection();
        void handleClient(int fd, uint32_t events);
        void closeConnection(int fd);

        int port_;
        int listen_fd_ = -1;
        int epoll_fd_ = -1;
        std::atomic<bool> running_{false};

        std::shared_ptr<Store> store_;
        std::map<int, std::unique_ptr<Connection>> connections_;
    };

} // namespace kvstore

#endif // KVSTORE_SERVER_H
