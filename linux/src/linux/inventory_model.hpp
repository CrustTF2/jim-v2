#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace LinuxSteam {
struct InventoryAttribute {
    uint32_t definition = 0;
    std::vector<uint8_t> value_bytes;
};

struct EquippedState {
    uint32_t player_class = 0;
    uint32_t slot = 0;
};

struct InventoryItem {
    uint64_t id = 0;
    uint32_t inventory = 0;
    uint32_t definition = 0;
    uint32_t quantity = 0;
    uint32_t level = 0;
    uint32_t quality = 0;
    uint32_t flags = 0;
    uint32_t origin = 0;
    std::string custom_name;
    std::string custom_description;
    std::vector<InventoryAttribute> attributes;
    std::vector<EquippedState> equipped;
    // Preserves all attributes and equipped state for the existing presentation code.
    std::vector<uint8_t> protobuf;
};

// Parses an already owner-verified SOCacheSubscribed payload transactionally.
// Item identifiers and custom text remain in memory and are never logged here.
bool parse_inventory_snapshot(const std::vector<uint8_t>& payload,
                              std::vector<InventoryItem>& output,
                              std::string& error);
}
