#include "server/server.h"
#include <iostream>
#include <csignal>
#include <cstdlib>

static kvstore::Server* g_server = nullptr; // ✅ capital "S"

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutting down server..." << std::endl;
        if (g_server) {
            g_server->stop();
        }
        exit(0);
    }
}

int main(int argc, char* argv[]) {
    int port = 6379;

    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            std::cerr << "Invalid port number" << std::endl;
            return 1;
        }
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    signal(SIGPIPE, SIG_IGN);

    kvstore::Server server(port); // ✅ capital "S"
    g_server = &server;

    server.run();

    return 0;
}
