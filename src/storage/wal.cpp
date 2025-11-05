//
// Created by Owner on 11/4/2025.
//
#include "wal.h"
#include <iostream>
#include <vector>
#include <arpa/inet.h>

namespace kvstore {

WAL::WAL(const std::string& filename) : filename_(filename) {
    file_.open(filename_, std::ios::binary | std::ios::app);
    if (!file_.is_open()) {
        std::cerr << "Failed to open WAL file: " << filename_ << std::endl;
    }
}

WAL::~WAL() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool WAL::writeEntry(WALOperation op, const std::string& key, const std::string& value) {
    if (!file_.is_open()) return false;

    std::lock_guard<std::mutex> lock(mutex_);

    // Write operation type
    uint8_t op_byte = static_cast<uint8_t>(op);
    file_.write(reinterpret_cast<const char*>(&op_byte), 1);

    // Write key length and key
    uint32_t key_len = htonl(key.size());
    file_.write(reinterpret_cast<const char*>(&key_len), 4);
    file_.write(key.data(), key.size());

    // Write value length and value
    uint32_t value_len = htonl(value.size());
    file_.write(reinterpret_cast<const char*>(&value_len), 4);
    file_.write(value.data(), value.size());

    // Flush to disk for durability
    file_.flush();

    return file_.good();
}

bool WAL::logSet(const std::string& key, const std::string& value) {
    return writeEntry(WALOperation::SET, key, value);
}

bool WAL::logDelete(const std::string& key) {
    return writeEntry(WALOperation::DELETE, key, "");
}

void WAL::sync() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.flush();
    }
}

std::vector<WAL::Entry> WAL::replay(const std::string& filename) {
    std::vector<Entry> entries;

    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cout << "No WAL file found, starting fresh" << std::endl;
        return entries;
    }

    std::cout << "Replaying WAL from: " << filename << std::endl;

    while (file.good() && file.peek() != EOF) {
        Entry entry;

        // Read operation type
        uint8_t op_byte;
        file.read(reinterpret_cast<char*>(&op_byte), 1);
        if (!file.good()) break;
        entry.op = static_cast<WALOperation>(op_byte);

        // Read key length and key
        uint32_t key_len;
        file.read(reinterpret_cast<char*>(&key_len), 4);
        if (!file.good()) break;
        key_len = ntohl(key_len);

        entry.key.resize(key_len);
        file.read(&entry.key[0], key_len);
        if (!file.good()) break;

        // Read value length and value
        uint32_t value_len;
        file.read(reinterpret_cast<char*>(&value_len), 4);
        if (!file.good()) break;
        value_len = ntohl(value_len);

        entry.value.resize(value_len);
        file.read(&entry.value[0], value_len);
        if (!file.good()) break;

        entries.push_back(entry);
    }

    std::cout << "Replayed " << entries.size() << " entries from WAL" << std::endl;

    return entries;
}

} // namespace kvstore