#include "../github_release.hpp"
#include <cstdio>
#include <stdexcept>
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main() { try {
    Json::Value release(Json::objectValue);
    release["tag_name"] = "v2.0.3"; release["draft"] = false; release["prerelease"] = false;
    check(is_newer_github_release(release, "Version 2.0.2"), "New release without assets was not selected");
    check(!is_newer_github_release(release, "Version 2.0.3"), "Equal version was selected");
    check(!is_newer_github_release(release, "Version 2.1.0"), "Older version was selected");
    auto copy = release; copy["prerelease"] = true;
    check(!is_newer_github_release(copy, "Version 2.0.2"), "Prerelease was selected");
    copy = release; copy["draft"] = true;
    check(!is_newer_github_release(copy, "Version 2.0.2"), "Draft was selected");
    copy = release; copy["tag_name"] = "v2.0.3-beta";
    check(!is_newer_github_release(copy, "Version 2.0.2"), "Invalid version was selected");
    copy = release; copy["draft"] = "false";
    check(!is_newer_github_release(copy, "Version 2.0.2"), "Invalid release data was selected");
    copy = release; copy.removeMember("tag_name");
    check(!is_newer_github_release(copy, "Version 2.0.2"), "Missing tag was selected");
    check(!is_newer_github_release(release, "not a version"), "Invalid current version was accepted");
    release["assets"] = Json::Value(Json::arrayValue);
    check(is_newer_github_release(release, "Version 2.0.2"), "Empty assets prevented selection");
    std::puts("GitHub release version checks passed."); return 0;
} catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; } }
