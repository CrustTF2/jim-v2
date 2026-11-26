#ifndef ITEM_PRESENTATION_HPP
#define ITEM_PRESENTATION_HPP

#include <json/json.h>
#include <string>
#include <vector>
#include <cstdint>

struct PresentationAttribute {
    unsigned int index;
    std::vector<char> bytes;
};

enum class ItemBadge { Halloween, Pyroland, Unusual, StatClock, Strange };

struct ItemSpell { std::string name; unsigned int colour; int paint_index; };

struct ItemPresentation {
    std::string name;
    std::string finish;
    std::string effects;
    std::vector<std::string> particle_effects;
    std::string wear;
    std::string custom_description;
    std::string gift_name;
    std::string crafter_name;
    std::string australium_image;
    std::string restriction_note;
    std::string bond_name_1;
    std::string bond_name_2;
    unsigned int bond_account_1 = 0;
    unsigned int bond_account_2 = 0;
    bool force_unique = false;
    unsigned int crafter_account = 0;
    bool australium = false;
    bool festivized = false;
    unsigned int gift_account = 0;
    unsigned int gift_timestamp = 0;
    int paint_spell_index = -1;
    bool pyrovision_restricted = false;
    std::vector<ItemSpell> spells;
    std::string sheen;
    std::string paint_colour_name;
    std::string killstreak_prefix;
    unsigned int paint_colour_rgb = 0;
    unsigned int sheen_colour = 0xFFFFFF;
    unsigned int killstreak_tier = 0;
    std::string killstreaker;
    std::string recipe_output;
    std::vector<std::string> recipe_inputs;
    unsigned int kit_target = 0;
    bool is_killstreak_kit = false;
    bool is_fabricator = false;
    unsigned int grade = 0;
    unsigned int paint_id = 0;
    float wear_value = 0;
    bool has_wear = false;
    bool decorated = false;
    bool unusual = false;
    bool strange = false;
    bool stat_clock = false;
    unsigned int stat_clock_count = 0;
    bool halloween_restricted = false;
    std::uint64_t decal_handle = 0;
    float stat_clock_scale = 1.0f;
    std::vector<ItemBadge> badges() const;
    static const char* grade_name(unsigned int grade);
    static unsigned int grade_colour(unsigned int grade);
};

// Presentation only: never changes the inventory messages sent to Steam.
class ItemPresentationSchema {
public:
    bool load(const char* path);
    ItemPresentation resolve(unsigned int definition, unsigned int quality,
        const std::vector<PresentationAttribute>& attributes, const char* fallback_name,
        unsigned int origin=0, unsigned int flags=0) const;
private:
    Json::Value data_;
};

#endif
