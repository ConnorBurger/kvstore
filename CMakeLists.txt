cmake_minimum_required(VERSION 3.15)
project(kvstore)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread")

# Server executable
add_executable(kvstore_server
        main.cpp
        src/server/server.cpp
        src/server/connection.cpp
        src/storage/store.cpp
        src/protocol/protool.cpp            # <-- fixed filename
)

target_include_directories(kvstore_server PRIVATE src)

# Client executable
add_executable(kvstore_client
        src/clinet/clinet.cpp      # <-- keeping your folder name as is
        src/protocol/protool.cpp            # <-- fixed filename
)

target_include_directories(kvstore_client PRIVATE src)
