#include "steam_runtime.hpp"
#include <dlfcn.h>
#include <utility>

namespace LinuxSteam {

Runtime::~Runtime() { close(); }

void Runtime::close() {
    if (library_) dlclose(library_);
    library_ = nullptr;
    factory_ = nullptr;
    get_callback_ = nullptr;
    free_callback_ = nullptr;
    loaded_path_.clear();
}

bool Runtime::load(const std::vector<std::string>& candidates, std::string& error) {
    close();
    error.clear();
    for (const auto& path : candidates) {
        void* library = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!library) continue;
        auto factory = reinterpret_cast<Factory>(dlsym(library, "CreateInterface"));
        auto get = reinterpret_cast<GetCallback>(dlsym(library, "Steam_BGetCallback"));
        auto release = reinterpret_cast<FreeCallback>(dlsym(library, "Steam_FreeLastCallback"));
        if (!factory || !get || !release) {
            dlclose(library);
            continue;
        }
        library_ = library;
        factory_ = factory;
        get_callback_ = get;
        free_callback_ = release;
        loaded_path_ = path;
        return true;
    }
    error = "Could not load the native Steam client. Start Steam and check its installation.";
    return false;
}

std::vector<std::string> default_client_paths(const std::string& home) {
    if (home.empty()) return {};
    return {home + "/.steam/sdk64/steamclient.so",
            home + "/.steam/root/linux64/steamclient.so",
            home + "/.local/share/Steam/linux64/steamclient.so",
            home + "/.var/app/com.valvesoftware.Steam/.local/share/Steam/linux64/steamclient.so"};
}
}
