#include "connection.h"
#include "../storage/store.h"
#include "../protocol/protocol.h"
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <cstring>
#include <cerrno>

namespace kvstore {

Connection::Connection(int fd, std::shared_ptr<Store> store)
    : fd_(fd), store_(store) {
}

Connection::~Connection() {
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

bool Connection::tryReadMessageLength() {
    if (expected_msg_len_ == 0 && read_buffer_.size() >= 4) {
        // Protocol::readUint32 should read a big-endian or little-endian length depending on your protocol
        expected_msg_len_ = Protocol::readUint32(read_buffer_, 0);
        // sanity limit (adjust as needed)
        if (expected_msg_len_ > 1024 * 1024) {
            std::cerr << "Message too large: " << expected_msg_len_ << std::endl;
            return false;
        }
    }
    return true;
}

bool Connection::handleRead() {
    char buffer[4096];

    while (true) {
        ssize_t n = recv(fd_, buffer, sizeof(buffer), 0);

        if (n > 0) {
            read_buffer_.insert(read_buffer_.end(), buffer, buffer + n);

            if (!tryReadMessageLength()) {
                return false;
            }

            // Process all complete messages in buffer
            while (expected_msg_len_ > 0 &&
                   read_buffer_.size() >= static_cast<size_t>(4 + expected_msg_len_)) {
                processRequest();

                // remove processed message (4 bytes length + payload)
                read_buffer_.erase(read_buffer_.begin(),
                                   read_buffer_.begin() + 4 + expected_msg_len_);
                expected_msg_len_ = 0;

                if (!tryReadMessageLength()) {
                    return false;
                }
            }
        } else if (n == 0) {
            // peer closed connection
            return false;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // no more data (non-blocking)
                break;
            } else if (errno == EINTR) {
                continue;
            } else {
                std::cerr << "recv error: " << strerror(errno) << std::endl;
                return false;
            }
        }
    }

    return true;
}

void Connection::processRequest() {
    Protocol::Request req;
    if (!Protocol::deserializeRequest(read_buffer_, req)) {
        Protocol::Response resp;
        resp.status = StatusCode::ERROR;
        resp.error_msg = "Invalid request format";

        auto data = Protocol::serializeResponse(resp);
        write_buffer_.insert(write_buffer_.end(), data.begin(), data.end());
        return;
    }

    Protocol::Response resp;

    switch (req.type) {
        case CommandType::SET: {
            store_->set(req.key, req.value);
            resp.status = StatusCode::OK;
            resp.data = "OK";
            break;
        }

        case CommandType::GET: {
            auto value = store_->get(req.key);
            if (value) {
                resp.status = StatusCode::OK;
                resp.data = *value;
            } else {
                resp.status = StatusCode::NOT_FOUND;
                resp.error_msg = "Key not found";
            }
            break;
        }

        case CommandType::DELETE: {
            bool removed = store_->remove(req.key);
            if (removed) {
                resp.status = StatusCode::OK;
                resp.data = "OK";
            } else {
                resp.status = StatusCode::NOT_FOUND;
                resp.error_msg = "Key not found";
            }
            break;
        }

        case CommandType::PING: {
            resp.status = StatusCode::OK;
            resp.data = "PONG";
            break;
        }

        default: {
            resp.status = StatusCode::ERROR;
            resp.error_msg = "Unknown command";
            break;
        }
    }

    auto data = Protocol::serializeResponse(resp);
    write_buffer_.insert(write_buffer_.end(), data.begin(), data.end());
}

bool Connection::handleWrite() {
    while (!write_buffer_.empty()) {
        ssize_t n = send(fd_, reinterpret_cast<const char*>(write_buffer_.data()),
                         write_buffer_.size(), 0);

        if (n > 0) {
            write_buffer_.erase(write_buffer_.begin(), write_buffer_.begin() + n);
        } else if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // socket not ready for write, try later
                break;
            } else if (errno == EINTR) {
                continue;
            } else {
                std::cerr << "send error: " << strerror(errno) << std::endl;
                return false;
            }
        }
    }

    return true;
}

} // namespace kvstore
