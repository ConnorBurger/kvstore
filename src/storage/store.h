#pragma once

#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <optional>

namespace kvstore {

class Store {
public:
    Store() = default;

    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    bool remove(const std::string& key);
    size_t size() const;
    void clear();

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::string> data_;
};

} // namespace kvstore