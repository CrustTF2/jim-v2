#pragma once
#include <json/json.h>
#include <array>
#include <cctype>
#include <string>

inline bool parse_release_version(const std::string& value, std::array<unsigned, 3>& out) {
    std::string s = value;
    if (s.compare(0, 8, "Version ") == 0) s.erase(0, 8);
    if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) s.erase(0, 1);
    size_t pos = 0;
    for (int i = 0; i < 3; ++i) {
        if (pos >= s.size() || !std::isdigit(static_cast<unsigned char>(s[pos]))) return false;
        unsigned n = 0;
        while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
            if (n > 1000000) return false;
            n = n * 10 + (s[pos++] - '0');
        }
        out[i] = n;
        if (i < 2 && (pos >= s.size() || s[pos++] != '.')) return false;
    }
    return pos == s.size();
}

// Release assets are intentionally irrelevant: the browser opens the release page.
inline bool is_newer_github_release(const Json::Value& root, const std::string& current) {
    if (!root.isObject() || !root["draft"].isBool() || !root["prerelease"].isBool() ||
        root["draft"].asBool() || root["prerelease"].asBool() || !root["tag_name"].isString())
        return false;
    std::array<unsigned, 3> installed{}, latest{};
    return parse_release_version(current, installed) &&
        parse_release_version(root["tag_name"].asString(), latest) && latest > installed;
}
