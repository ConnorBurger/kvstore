#pragma once

#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <memory>
#include "wal.h"

namespace kvstore {

    class Store {
    public:
        Store();
        explicit Store(const std::string& wal_filename);

        void set(const std::string& key, const std::string& value);
        std::optional<std::string> get(const std::string& key);
        bool remove(const std::string& key);
        size_t size() const;
        void clear();

        // Recover from WAL
        void recover();

    private:
        mutable std::shared_mutex mutex_;
        std::unordered_map<std::string, std::string> data_;
        std::string wal_filename_;
        std::unique_ptr<WAL> wal_;
    };

} // namespace kvstore