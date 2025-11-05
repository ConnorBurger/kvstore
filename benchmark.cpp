#include "protocol/protocol.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <vector>

class BenchmarkClient {
public:
    BenchmarkClient(const std::string& host, int port) : host_(host), port_(port) {}

    bool connect() {
        fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) return false;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);

        if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) <= 0) return false;
        if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) return false;

        return true;
    }

    void disconnect() {
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }

    bool set(const std::string& key, const std::string& value) {
        kvstore::Protocol::Request req;
        req.type = kvstore::CommandType::SET;
        req.key = key;
        req.value = value;

        auto data = kvstore::Protocol::serializeRequest(req);
        ssize_t sent = 0;
        while (sent < static_cast<ssize_t>(data.size())) {
            ssize_t n = send(fd_, data.data() + sent, data.size() - sent, 0);
            if (n < 0) return false;
            sent += n;
        }

        std::vector<uint8_t> buffer(4096);
        ssize_t n = recv(fd_, buffer.data(), buffer.size(), 0);
        if (n <= 0) return false;

        buffer.resize(n);
        kvstore::Protocol::Response resp;
        return kvstore::Protocol::deserializeResponse(buffer, resp);
    }

    bool get(const std::string& key) {
        kvstore::Protocol::Request req;
        req.type = kvstore::CommandType::GET;
        req.key = key;

        auto data = kvstore::Protocol::serializeRequest(req);
        ssize_t sent = 0;
        while (sent < static_cast<ssize_t>(data.size())) {
            ssize_t n = send(fd_, data.data() + sent, data.size() - sent, 0);
            if (n < 0) return false;
            sent += n;
        }

        std::vector<uint8_t> buffer(4096);
        ssize_t n = recv(fd_, buffer.data(), buffer.size(), 0);
        if (n <= 0) return false;

        buffer.resize(n);
        kvstore::Protocol::Response resp;
        return kvstore::Protocol::deserializeResponse(buffer, resp);
    }

private:
    std::string host_;
    int port_;
    int fd_ = -1;
};

void runBenchmark(const std::string& name, int num_ops) {
    std::cout << "\n=== " << name << " ===" << std::endl;

    BenchmarkClient client("127.0.0.1", 6379);

    if (!client.connect()) {
        std::cerr << "Failed to connect" << std::endl;
        return;
    }

    // Benchmark SET operations
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_ops; i++) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        if (!client.set(key, value)) {
            std::cerr << "SET failed at " << i << std::endl;
            break;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double ops_per_sec = (num_ops * 1000.0) / duration.count();
    std::cout << "SET: " << num_ops << " ops in " << duration.count() << " ms" << std::endl;
    std::cout << "     " << static_cast<int>(ops_per_sec) << " ops/sec" << std::endl;

    // Benchmark GET operations
    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_ops; i++) {
        std::string key = "key" + std::to_string(i);
        if (!client.get(key)) {
            std::cerr << "GET failed at " << i << std::endl;
            break;
        }
    }

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    ops_per_sec = (num_ops * 1000.0) / duration.count();
    std::cout << "GET: " << num_ops << " ops in " << duration.count() << " ms" << std::endl;
    std::cout << "     " << static_cast<int>(ops_per_sec) << " ops/sec" << std::endl;

    client.disconnect();
}

int main(int argc, char* argv[]) {
    int num_ops = 10000;

    if (argc > 1) {
        num_ops = std::atoi(argv[1]);
    }

    std::cout << "KVStore Benchmark" << std::endl;
    std::cout << "=================" << std::endl;
    std::cout << "Operations: " << num_ops << std::endl;

    runBenchmark("Benchmark", num_ops);

    return 0;
}