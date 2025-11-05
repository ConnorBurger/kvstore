#include "../protocol/protocol.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

class Client {
public:
    Client(const std::string& host, int port) : host_(host), port_(port) {}

    bool connect() {
        fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) {
            std::cerr << "socket error: " << strerror(errno) << std::endl;
            return false;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);

        if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) <= 0) {
            std::cerr << "Invalid address" << std::endl;
            return false;
        }

        if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            std::cerr << "connect error: " << strerror(errno) << std::endl;
            return false;
        }

        std::cout << "Connected to " << host_ << ":" << port_ << std::endl;
        return true;
    }

    void disconnect() {
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }

    bool sendRequest(const kvstore::Protocol::Request& req,
                     kvstore::Protocol::Response& resp) {
        auto data = kvstore::Protocol::serializeRequest(req);

        ssize_t sent = 0;
        while (sent < static_cast<ssize_t>(data.size())) {
            ssize_t n = send(fd_, data.data() + sent, data.size() - sent, 0);
            if (n < 0) {
                std::cerr << "send error: " << strerror(errno) << std::endl;
                return false;
            }
            sent += n;
        }

        std::vector<uint8_t> buffer(4096);
        ssize_t n = recv(fd_, buffer.data(), buffer.size(), 0);
        if (n <= 0) {
            std::cerr << "recv error: " << strerror(errno) << std::endl;
            return false;
        }

        buffer.resize(n);

        if (!kvstore::Protocol::deserializeResponse(buffer, resp)) {
            std::cerr << "Failed to deserialize response" << std::endl;
            return false;
        }

        return true;
    }

    void runInteractive() {
        std::cout << "\nKVStore Client\n";
        std::cout << "Commands: SET key value, GET key, DELETE key, PING, QUIT\n\n";

        std::string line;
        while (true) {
            std::cout << "kvstore> ";
            if (!std::getline(std::cin, line)) {
                break;
            }

            if (line.empty()) continue;

            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;

            for (char& c : cmd) c = std::toupper(c);

            if (cmd == "QUIT" || cmd == "EXIT") {
                break;
            }

            kvstore::Protocol::Request req;
            kvstore::Protocol::Response resp;

            if (cmd == "SET") {
                iss >> req.key >> req.value;
                if (req.key.empty() || req.value.empty()) {
                    std::cout << "Usage: SET key value\n";
                    continue;
                }
                req.type = kvstore::CommandType::SET;
            } else if (cmd == "GET") {
                iss >> req.key;
                if (req.key.empty()) {
                    std::cout << "Usage: GET key\n";
                    continue;
                }
                req.type = kvstore::CommandType::GET;
            } else if (cmd == "DELETE" || cmd == "DEL") {
                iss >> req.key;
                if (req.key.empty()) {
                    std::cout << "Usage: DELETE key\n";
                    continue;
                }
                req.type = kvstore::CommandType::DELETE;
            } else if (cmd == "PING") {
                req.type = kvstore::CommandType::PING;
            } else {
                std::cout << "Unknown command: " << cmd << "\n";
                continue;
            }

            if (sendRequest(req, resp)) {
                if (resp.status == kvstore::StatusCode::OK) {
                    std::cout << resp.data << "\n";
                } else if (resp.status == kvstore::StatusCode::NOT_FOUND) {
                    std::cout << "(nil)\n";
                } else {
                    std::cout << "Error: " << resp.error_msg << "\n";
                }
            }
        }
    }

private:
    std::string host_;
    int port_;
    int fd_ = -1;
};

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int port = 6379;

    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = std::atoi(argv[2]);
    }

    Client client(host, port);

    if (!client.connect()) {
        return 1;
    }

    client.runInteractive();
    client.disconnect();

    return 0;
}
