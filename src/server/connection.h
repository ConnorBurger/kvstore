#pragma once

#include <vector>
#include <cstdint>
#include <memory>

namespace kvstore {

    class Store;

    class Connection {
    public:
        explicit Connection(int fd, std::shared_ptr<Store> store);
        ~Connection();

        // non-copyable
        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;

        int fd() const { return fd_; }

        bool handleRead();
        bool handleWrite();
        bool hasDataToWrite() const { return !write_buffer_.empty(); }

    private:
        int fd_;
        std::shared_ptr<Store> store_;

        std::vector<uint8_t> read_buffer_;
        std::vector<uint8_t> write_buffer_;
        uint32_t expected_msg_len_ = 0;

        void processRequest();
        bool tryReadMessageLength();
    };

} // namespace kvstore
