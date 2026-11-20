#pragma once
#include <string>
#include <vector>

namespace LinuxSteam {

// Owns a Linux Steam client library. Interface pointers must not outlive it.
class Runtime {
public:
    using Factory = void* (*)(const char*, int*);
    using GetCallback = bool (*)(int, void*, unsigned long long*);
    using FreeCallback = void (*)(int);

    Runtime() = default;
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    bool load(const std::vector<std::string>& candidates, std::string& error);
    void close();
    Factory factory() const { return factory_; }
    GetCallback get_callback() const { return get_callback_; }
    FreeCallback free_callback() const { return free_callback_; }
    const std::string& loaded_path() const { return loaded_path_; }

private:
    void* library_ = nullptr;
    Factory factory_ = nullptr;
    GetCallback get_callback_ = nullptr;
    FreeCallback free_callback_ = nullptr;
    std::string loaded_path_;
};

// Locations used by native Steam installations, including Flatpak.
std::vector<std::string> default_client_paths(const std::string& home);
}
