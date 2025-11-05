#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace kvstore {

// Command types
enum class CommandType : uint8_t {
    SET = 1,
    GET = 2,
    DELETE = 3,
    PING = 4
};

// Response status
enum class StatusCode : uint8_t {
    OK = 0,
    ERROR = 1,
    NOT_FOUND = 2
};

class Protocol {
public:
    struct Request {
        CommandType type;
        std::string key;
        std::string value;
    };

    struct Response {
        StatusCode status;
        std::string data;
        std::string error_msg;
    };

    static std::vector<uint8_t> serializeRequest(const Request& req);
    static bool deserializeRequest(const std::vector<uint8_t>& data, Request& req);
    static std::vector<uint8_t> serializeResponse(const Response& resp);
    static bool deserializeResponse(const std::vector<uint8_t>& data, Response& resp);

    // Helper functions
    static void writeUint32(std::vector<uint8_t>& buf, uint32_t val);
    static uint32_t readUint32(const std::vector<uint8_t>& buf, size_t offset);
    static void writeString(std::vector<uint8_t>& buf, const std::string& str);
    static bool readString(const std::vector<uint8_t>& buf, size_t& offset, std::string& str);
};

} // namespace kvstore