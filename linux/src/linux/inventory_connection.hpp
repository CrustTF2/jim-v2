#pragma once
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace LinuxSteam {

// Read-only connection for the native Linux application. Payloads are passed
// directly to the caller in memory; this class never writes inventory to disk.
class InventoryConnection {
public:
    using InventoryHandler = std::function<void(const std::vector<uint8_t>&, size_t, bool)>;
    InventoryConnection() = default;
    ~InventoryConnection();
    InventoryConnection(const InventoryConnection&) = delete;
    InventoryConnection& operator=(const InventoryConnection&) = delete;

    bool open(const std::string& client_path, const std::string& api_path, std::string& error);
    bool poll(const InventoryHandler& on_inventory, std::string& error);
    void close();
    bool has_inventory() const { return has_inventory_; }

private:
    using InitFn = bool (*)();
    using VoidFn = void (*)();
    void* api_ = nullptr;
    void* client_ = nullptr;
    void* coordinator_ = nullptr;
    VoidFn shutdown_ = nullptr;
    VoidFn callbacks_ = nullptr;
    uint64_t owner_ = 0;
    bool initialized_ = false;
    bool welcomed_ = false;
    bool refresh_sent_ = false;
    bool has_inventory_ = false;
    std::chrono::steady_clock::time_point next_hello_;
};
}
