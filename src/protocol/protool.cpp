#include "protocol.h"
#include <cstring>
#include <arpa/inet.h>

namespace kvstore {

void Protocol::writeUint32(std::vector<uint8_t>& buf, uint32_t val) {
    uint32_t net_val = htonl(val);
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&net_val);
    buf.insert(buf.end(), bytes, bytes + 4);
}

uint32_t Protocol::readUint32(const std::vector<uint8_t>& buf, size_t offset) {
    if (offset + 4 > buf.size()) return 0;
    uint32_t net_val;
    std::memcpy(&net_val, buf.data() + offset, 4);
    return ntohl(net_val);
}

void Protocol::writeString(std::vector<uint8_t>& buf, const std::string& str) {
    writeUint32(buf, str.size());
    buf.insert(buf.end(), str.begin(), str.end());
}

bool Protocol::readString(const std::vector<uint8_t>& buf, size_t& offset, std::string& str) {
    if (offset + 4 > buf.size()) return false;

    uint32_t len = readUint32(buf, offset);
    offset += 4;

    if (offset + len > buf.size()) return false;

    str.assign(buf.begin() + offset, buf.begin() + offset + len);
    offset += len;
    return true;
}

std::vector<uint8_t> Protocol::serializeRequest(const Request& req) {
    std::vector<uint8_t> payload;

    payload.push_back(static_cast<uint8_t>(req.type));
    writeString(payload, req.key);

    if (req.type == CommandType::SET) {
        writeString(payload, req.value);
    }

    std::vector<uint8_t> result;
    writeUint32(result, payload.size());
    result.insert(result.end(), payload.begin(), payload.end());

    return result;
}

bool Protocol::deserializeRequest(const std::vector<uint8_t>& data, Request& req) {
    if (data.size() < 5) return false;

    uint32_t msg_len = readUint32(data, 0);
    if (data.size() < 4 + msg_len) return false;

    size_t offset = 4;
    req.type = static_cast<CommandType>(data[offset++]);

    if (!readString(data, offset, req.key)) return false;

    if (req.type == CommandType::SET) {
        if (!readString(data, offset, req.value)) return false;
    }

    return true;
}

std::vector<uint8_t> Protocol::serializeResponse(const Response& resp) {
    std::vector<uint8_t> result;

    result.push_back(static_cast<uint8_t>(resp.status));

    const std::string& payload = (resp.status == StatusCode::OK) ? resp.data : resp.error_msg;
    writeString(result, payload);

    return result;
}

bool Protocol::deserializeResponse(const std::vector<uint8_t>& data, Response& resp) {
    if (data.size() < 5) return false;

    size_t offset = 0;
    resp.status = static_cast<StatusCode>(data[offset++]);

    std::string payload;
    if (!readString(data, offset, payload)) return false;

    if (resp.status == StatusCode::OK) {
        resp.data = payload;
    } else {
        resp.error_msg = payload;
    }

    return true;
}

} // namespace kvstore