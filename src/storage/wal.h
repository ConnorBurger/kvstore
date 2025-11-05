#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <cstdint>
#include <vector>

namespace kvstore {

    enum class WALOperation : uint8_t {
        SET = 1,
        DELETE = 2
    };

    class WAL {
    public:
        explicit WAL(const std::string& filename);
        ~WAL();

        // Log a SET operation
        bool logSet(const std::string& key, const std::string& value);

        // Log a DELETE operation
        bool logDelete(const std::string& key);

        // Replay the log to recover state
        // Returns pairs of (operation, key, value)
        struct Entry {
            WALOperation op;
            std::string key;
            std::string value;
        };

        static std::vector<Entry> replay(const std::string& filename);

        // Sync to disk
        void sync();

    private:
        std::string filename_;
        std::ofstream file_;
        std::mutex mutex_;

        // Write entry format: [op(1 byte)][key_len(4)][key][value_len(4)][value]
        bool writeEntry(WALOperation op, const std::string& key, const std::string& value);
    };

} // namespace kvstore