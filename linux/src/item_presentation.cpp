#include "item_presentation.hpp"
#include <fstream>
#include <map>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <set>
#include <ctime>
#include "protobuf/tf_gcmessages.pb.h"

std::vector<ItemBadge> ItemPresentation::badges() const
{
    std::vector<ItemBadge> result;
    if (halloween_restricted) result.push_back(ItemBadge::Halloween);
    if (pyrovision_restricted) result.push_back(ItemBadge::Pyroland);
    if (unusual) result.push_back(ItemBadge::Unusual);
    // In TF2, Stat-Clock is the Strange badge variant for supported painted items.
    if (strange) result.push_back(stat_clock ? ItemBadge::StatClock : ItemBadge::Strange);
    return result;
}

const char* ItemPresentation::grade_name(unsigned int grade)
{
    static const char* names[] = {"", "Civilian Grade", "Freelance Grade", "Mercenary Grade",
        "Commando Grade", "Assassin Grade", "Elite Grade"};
    return grade < 7 ? names[grade] : "";
}

unsigned int ItemPresentation::grade_colour(unsigned int grade)
{
    static const unsigned int colours[] = {0xFFD700, 0xB0C3D9, 0x5E98D9, 0x4B69FF,
        0x8847FF, 0xD32CE6, 0xEB4B4B};
    return grade < 7 ? colours[grade] : colours[0];
}

bool ItemPresentationSchema::load(const char* path)
{
    std::ifstream stream(path);
    Json::Value parsed;
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!stream || !Json::parseFromStream(builder, stream, &parsed, &errors) ||
        parsed.get("format",0).asInt() != 1 || !parsed["items"].isObject() ||
        !parsed["paints"].isObject() || !parsed["effects"].isObject()) {
        data_ = Json::Value();
        return false;
    }
    data_ = parsed;
    return true;
}

ItemPresentation ItemPresentationSchema::resolve(unsigned int definition, unsigned int quality,
    const std::vector<PresentationAttribute>& attributes, const char* fallback_name, unsigned int origin, unsigned int flags) const
{
    ItemPresentation result;
    const Json::Value& item = data_["items"][std::to_string(definition)];
    std::map<unsigned int, double> values;
    std::set<unsigned int> present, instance_present;
    bool stat_clock_capable = item.get("stat_clock_capable",false).asBool();
    bool has_instance_paint = false;
    result.halloween_restricted = item.get("halloween_restricted",false).asBool();
    result.pyrovision_restricted = item.get("pyrovision_restricted",false).asBool();
    result.stat_clock_scale = item.get("stat_clock_scale",1.0).asFloat();
    for (const auto& key : item["attributes"].getMemberNames()) {
        if (item["attributes"][key].isNumeric()) { auto id=static_cast<unsigned int>(std::stoul(key));values[id]=item["attributes"][key].asDouble();present.insert(id); }
    }
    // TF2 stores paint IDs and taunt effects as uint32, weapon/hat effects and wear as float.
    // Instance attributes override schema defaults, including explicit zero values.
    for (const auto& attribute : attributes) {
        if (attribute.index == 719 && attribute.bytes.size() > 2) stat_clock_capable = true;
        if (attribute.index == 501 && !attribute.bytes.empty() && attribute.bytes.size()<=65536) {
            CAttribute_String text;
            if (text.ParseFromArray(attribute.bytes.data(),static_cast<int>(attribute.bytes.size())) && text.has_value()) result.custom_description=text.value();
        }
        if (attribute.bytes.size() != 4) continue;
        present.insert(attribute.index);
        instance_present.insert(attribute.index);
        if (attribute.index==142) has_instance_paint=true;
        double number;
        switch (attribute.index) {
        case 152: case 185: case 186: case 211: case 228: case 302: case 372: case 373: case 785: case 2027: case 2051: case 2052: case 227: case 834: case 2041: case 214: case 294: case 379: case 381: case 383: {
            std::uint32_t value;
            std::memcpy(&value, attribute.bytes.data(), sizeof(value));
            number = value;
            break;
        }
        case 142: case 153: case 195: case 261: case 449: case 777: case 2028: case 2053: case 1004: case 1005: case 1006: case 1007: case 1008: case 1009: case 134: case 189: case 725: case 749: case 2012: case 2013: case 2014: case 2025: {
            float value;
            std::memcpy(&value, attribute.bytes.data(), sizeof(value));
            number = value;
            break;
        }
        default: continue;
        }
        if (std::isfinite(number) && number >= 0 && number <= 4294967295.0) values[attribute.index] = number;
    }
    if (values.count(152) && values.count(227))
        result.decal_handle = (static_cast<std::uint64_t>(values[227]) << 32) | static_cast<std::uint32_t>(values[152]);
    auto positive = [&](unsigned int index) { return values.count(index) && values[index] > 0; };
    if (values.count(228)) result.crafter_account=static_cast<unsigned int>(values[228]);
    if (values.count(372)) result.bond_account_1=static_cast<unsigned int>(values[372]);
    if (values.count(373)) result.bond_account_2=static_cast<unsigned int>(values[373]);
    result.festivized=present.count(2053)!=0;
    result.australium=present.count(2027)!=0;
    if (result.australium) result.australium_image=item.get("australium_image","").asString();
    if (values.count(186)) result.gift_account=static_cast<unsigned int>(values[186]);
    if (result.gift_account && values.count(185)) result.gift_timestamp=static_cast<unsigned int>(values[185]);
    if (values.count(1004)) {
        unsigned int id=static_cast<unsigned int>(values[1004]);
        const Json::Value& spell=data_["paint_spells"][std::to_string(id)];
        if (id<5 && spell.isString() && !spell.asString().empty()) {
            result.paint_spell_index=static_cast<int>(id);
            result.spells.push_back({spell.asString(),0xFFFFFF,static_cast<int>(id)});
        }
    }
    if (values.count(1005)) {
        unsigned int id=static_cast<unsigned int>(values[1005]);
        const Json::Value& spell=data_["footprint_spells"][std::to_string(id)];
        if (spell.isString() && !spell.asString().empty()) {
            unsigned int colour=id>2 ? id : id==1 ? 0xB8383B : 0xA76AD1;
            result.spells.push_back({spell.asString(),colour,-1});
        }
    }
    const unsigned int spell_ids[]={1006,1007,1008,1009};
    const char* spell_names[]={"Voices from Below","Pumpkin Bombs","Halloween Fire","Exorcism"};
    const unsigned int spell_colours[]={0xA680B8,0xFF8C00,0x32CD32,0xA680B8};
    for (int i=0;i<4;++i) if (positive(spell_ids[i])) result.spells.push_back({spell_names[i],spell_colours[i],-1});
    // The World Traveler has a built-in particle effect but remains Unique.
    result.force_unique = definition==1899;
    result.unusual = !result.force_unique && (quality == 5 || positive(134) || positive(2041));
    result.strange = quality == 11 || (values.count(189) && values[189] == 11) ||
        values.count(214) || values.count(294) || values.count(379) || values.count(381) || values.count(383);
    if (result.force_unique) result.strange=false;
    result.stat_clock = result.strange && positive(834) && stat_clock_capable;
    if (values.count(214)) result.stat_clock_count = static_cast<unsigned int>(values[214]);
    result.grade = item.get("grade",0).asUInt();
    result.decorated = quality == 15 || item.get("decorated",false).asBool() || positive(834);
    if (result.decorated) {
        result.name = item.get("name","").asString();
        if (result.name.empty()) {
            result.name = fallback_name;
            // The legacy web schema may supply this label as part of the base name.
            const std::string prefix = "Decorated Weapon ";
            if (result.name.compare(0,prefix.size(),prefix)==0) result.name.erase(0,prefix.size());
        }
        if (values.count(834)) {
            const unsigned int id = static_cast<unsigned int>(values[834]);
            result.paint_id = id;
            const Json::Value& paint = data_["paints"][std::to_string(id)];
            if (paint["name"].isString()) result.finish = paint["name"].asString();
            else if (id) result.finish = "Unknown War Paint #" + std::to_string(id);
            const unsigned int grade = paint.get("grade",0).asUInt();
            if (grade) result.grade = grade;
            else if (item["attributes"].get("834",0).asUInt() != id) result.grade = 0;
            if (!result.finish.empty()) result.name = result.finish + " " + result.name;
        }
        auto wear = values.find(725);
        if (wear == values.end()) wear = values.find(749);
        if (wear != values.end() && wear->second <= 1) {
            const double w = wear->second;
            result.has_wear = true;
            result.wear_value = static_cast<float>(w);
            result.wear = w <= 0.2f ? "Factory New" : w <= 0.4f ? "Minimal Wear" :
                w <= 0.6f ? "Field-Tested" : w <= 0.8f ? "Well-Worn" : "Battle Scarred";
        }
    }
    // Dynamic recipe components are individual protobuf-valued attributes.
    // Their embedded key/value string uses the separator documented in TF2's SDK.
    auto item_name = [&](unsigned int id) {
        std::string name=data_["items"][std::to_string(id)].get("display_name","").asString();
        return name.empty() ? std::string("Item #")+std::to_string(id) : name;
    };
    auto tier_name = [](unsigned int tier) {
        return tier >= 3 ? std::string("Professional Killstreak") : tier == 2 ? std::string("Specialized Killstreak") : std::string("Killstreak");
    };
    auto effect_name = [&](const char* key,unsigned int id) {
        if (!id) return std::string();
        std::string name=data_[key][std::to_string(id)].asString();
        return name.empty() ? std::string("Unknown #")+std::to_string(id) : name;
    };
    auto field_number = [](const std::map<unsigned int,double>& fields,unsigned int id,unsigned int fallback=0) {
        auto i=fields.find(id);return i==fields.end() ? fallback : static_cast<unsigned int>(i->second);
    };
    auto kit_name = [&](const std::map<unsigned int,double>& fields) {
        const unsigned int tier=field_number(fields,2025,1), target=field_number(fields,2012);
        return tier_name(tier)+(target ? " "+item_name(target) : "")+" Kit";
    };
    auto decode_embedded = [](const std::string& encoded,std::map<unsigned int,double>& fields) {
        const std::string separator="|\x01\x02\x01\x03|\x01\x02\x01\x03|";
        std::vector<std::string> words;size_t pos=0;
        while (pos<encoded.size()) {
            size_t next=encoded.find(separator,pos);
            words.push_back(encoded.substr(pos,next==std::string::npos ? next : next-pos));
            if (next==std::string::npos) break;
            pos=next+separator.size();
        }
        for (size_t i=0;i+1<words.size();i+=2) {
            char* end=nullptr;const unsigned long id=std::strtoul(words[i].c_str(),&end,10);
            if (!end || *end || id>65535) continue;
            char* last=nullptr;double number=std::strtod(words[i+1].c_str(),&last);
            if (last && !*last && std::isfinite(number) && number>=0 && number<=4294967295.0) fields[static_cast<unsigned int>(id)]=number;
        }
    };
    // Weapon sheen colors are TF2's RED-team base shader colors.
    const unsigned int sheen_colours[]={0xFFFFFF,0xC8140F,0xF2AC0A,0xFF4B05,0x64FF0A,0x28FF46,0x6914FF,0xFF1EFF};
    const unsigned int sheen_id=field_number(values,2014);
    if (sheen_id<8) result.sheen_colour=sheen_colours[sheen_id];
    if (item.get("tool_type","").asString()!="paint_can" && has_instance_paint && values.count(142)) {
        const unsigned int rgb=field_number(values,142);
        const Json::Value& paint=data_["cosmetic_paints"][std::to_string(rgb)];
        result.paint_colour_rgb=paint.get("colour",rgb).asUInt();
        result.paint_colour_name=paint.get("name","").asString();
        if (result.paint_colour_name.empty()) result.paint_colour_name="Unknown paint #"+std::to_string(rgb);
    }
    result.is_killstreak_kit=item.get("tool_type","").asString()=="killstreakifier";
    if (result.is_killstreak_kit) {
        result.kit_target=field_number(values,2012);
        result.name=kit_name(values);
        result.sheen=effect_name("sheens",field_number(values,2014));
        result.killstreaker=effect_name("killstreakers",field_number(values,2013));
    }
    if (!result.is_killstreak_kit && item.get("tool_type","").asString()!="dynamic_recipe") {
        result.killstreak_tier=field_number(values,2025);
        if (result.killstreak_tier>0) {
            if (field_number(values,2013)) result.killstreak_tier=3;
            else if (field_number(values,2014) && result.killstreak_tier<2) result.killstreak_tier=2;
            result.killstreak_prefix=tier_name(result.killstreak_tier);
            result.sheen=effect_name("sheens",sheen_id);
            result.killstreaker=effect_name("killstreakers",field_number(values,2013));
        }
    }
    struct RecipeLine { unsigned int flags,definition,quality,required,fulfilled;std::map<unsigned int,double> fields; };
    std::vector<RecipeLine> components;
    if (item.get("tool_type","").asString()=="dynamic_recipe") {
        for (const auto& attribute : attributes) {
            if (attribute.index<2000 || attribute.index>2009 || attribute.bytes.empty() || attribute.bytes.size()>65536) continue;
            CAttribute_DynamicRecipeComponent recipe;
            if (!recipe.ParseFromArray(attribute.bytes.data(),static_cast<int>(attribute.bytes.size()))) continue;
            RecipeLine line{recipe.component_flags(),recipe.def_index(),recipe.item_quality(),recipe.num_required(),recipe.num_fulfilled(),{}};
            const Json::Value& defaults=data_["items"][std::to_string(line.definition)]["attributes"];
            for (const auto& key : defaults.getMemberNames()) if (defaults[key].isNumeric()) line.fields[static_cast<unsigned int>(std::stoul(key))]=defaults[key].asDouble();
            decode_embedded(recipe.attributes_string(),line.fields);
            components.push_back(line);
        }
        for (const auto& line : components) {
            if (!(line.flags&1)) continue;
            if (data_["items"][std::to_string(line.definition)].get("tool_type","").asString()!="killstreakifier") continue;
            result.is_fabricator=true;result.recipe_output=kit_name(line.fields);
            result.name=result.recipe_output+" Fabricator";
            const unsigned int output_sheen=field_number(line.fields,2014);
            result.sheen=effect_name("sheens",output_sheen);
            if (output_sheen<8) result.sheen_colour=sheen_colours[output_sheen];
            result.killstreaker=effect_name("killstreakers",field_number(line.fields,2013));
            break;
        }
        if (result.is_fabricator) for (const auto& line : components) {
            if ((line.flags&1) || line.fulfilled>=line.required) continue;
            const bool has_definition=(line.flags&4) && line.definition;
            std::string name=has_definition ? item_name(line.definition) : "Item";
            const unsigned int tier=field_number(line.fields,2025);
            if (tier) name=tier_name(tier)+" "+name;
            if ((line.flags&8) && (!has_definition || line.quality==11)) {
                if (line.quality==6) name="Unique "+name;
                else if (line.quality==11) name="Strange "+name;
            }
            result.recipe_inputs.push_back(name+" x "+std::to_string(line.required-line.fulfilled));
        }
    }
    // Honor explicit trade restrictions on both item definitions and instances.
    // Non-marketable by default does not itself produce a TF2 tag.
    const bool always=present.count(195)!=0;
    const bool preview=origin==17, loaner=origin==24 || instance_present.count(2051) || instance_present.count(2052);
    const bool trade_block=!always && ((flags&1)!=0 || present.count(153));
    const bool temporary_trade_block=!always && values.count(211) && values[211]>static_cast<double>(std::time(nullptr));
    const bool craft_block=!always && ((flags&2)!=0 || instance_present.count(449));
    const bool gift_block=instance_present.count(785)!=0;
    if (definition==1899) result.restriction_note="Not Tradable, Marketable, or Usable in Crafting";
    else if (preview) result.restriction_note="Test Run - Cannot be traded, marketed, crafted, or modified";
    else if (loaner) result.restriction_note="Loaner - Cannot be traded, marketed, crafted, or modified";
    else if (!always && origin==1) result.restriction_note="Achievement Item: Not Tradable or Marketable";
    else if (!always && origin==29) result.restriction_note="Free Contract Reward: Not Tradable or Marketable";
    else {
        std::vector<std::string> limitations;
        if (trade_block || temporary_trade_block) limitations.push_back("Tradable");
        if (trade_block) limitations.push_back("Marketable");
        if (craft_block) limitations.push_back("Usable in Crafting");
        if (gift_block) limitations.push_back("Gift Wrappable");
        if (!limitations.empty()) {
            result.restriction_note="Not ";
            for (size_t i=0;i<limitations.size();++i) {
                if (i) result.restriction_note += i+1==limitations.size() ? (limitations.size()>2 ? ", or " : " or ") : ", ";
                result.restriction_note += limitations[i];
            }
        }
    }
    if (result.grade > 6) result.grade = 0;
    for (unsigned int index : {134u,2041u}) {
        if (!positive(index)) continue;
        const unsigned int id = static_cast<unsigned int>(values[index]);
        const Json::Value& effect = data_["effects"][std::to_string(id)];
        const std::string name = effect.isString() ? effect.asString() : "Unknown effect #" + std::to_string(id);
        if (!result.effects.empty()) result.effects += "\n";
        result.effects += "Unusual Effect: " + name;
    }
    return result;
}
