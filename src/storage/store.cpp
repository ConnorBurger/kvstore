#include "store.h"
#include "wal.h"
#include <mutex>
#include <iostream>

namespace kvstore {

    Store::Store() : Store("kvstore.wal") {
    }

    Store::Store(const std::string& wal_filename)
        : wal_filename_(wal_filename), wal_(std::make_unique<WAL>(wal_filename)) {
        recover();
    }

    void Store::recover() {
        std::cout << "Starting recovery..." << std::endl;

        auto entries = WAL::replay(wal_filename_);

        for (const auto& entry : entries) {
            if (entry.op == WALOperation::SET) {
                data_[entry.key] = entry.value;
            } else if (entry.op == WALOperation::DELETE) {
                data_.erase(entry.key);
            }
        }

        std::cout << "Recovery complete. " << data_.size() << " keys in store." << std::endl;
    }

    void Store::set(const std::string& key, const std::string& value) {
        // Log to WAL BEFORE modifying data
        if (wal_) {
            wal_->logSet(key, value);
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);
        data_[key] = value;
    }

    std::optional<std::string> Store::get(const std::string& key) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = data_.find(key);
        if (it != data_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    bool Store::remove(const std::string& key) {
        // Log to WAL BEFORE modifying data
        if (wal_) {
            wal_->logDelete(key);
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);
        return data_.erase(key) > 0;
    }

    size_t Store::size() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return data_.size();
    }

    void Store::clear() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        data_.clear();
    }

} // namespace kvstore