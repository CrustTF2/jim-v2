#pragma once
#include <string>

namespace LinuxSteam {
struct Paths { std::string client; std::string api; std::string tf; };
// Overrides may be empty. Only probes paths; never changes the Steam installation.
Paths discover_paths(const std::string& home,const std::string& tf_override,
                     const std::string& client_override,const std::string& api_override);
}
