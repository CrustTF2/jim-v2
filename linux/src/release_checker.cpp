#include "release_checker.hpp"
#include "github_release.hpp"
#include <curl/curl.h>
#include <json/json.h>
#include <memory>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#else
#include <cerrno>
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
extern char** environ;
#endif

namespace {
const char* const release_api = "https://api.github.com/repos/CrustTF2/jim-v2/releases/latest";
const char* const release_page = "https://github.com/CrustTF2/jim-v2/releases";
const size_t maximum_response = 1024 * 1024;

size_t append_response(char* data, size_t size, size_t count, void* context) {
    if (size && count > maximum_response / size) return 0;
    const size_t length = size * count;
    std::string& response = *static_cast<std::string*>(context);
    if (length > maximum_response - response.size()) return 0;
    try { response.append(data, length); } catch (...) { return 0; }
    return length;
}

void open_release_page() {
#if defined(_WIN32)
    ShellExecuteA(nullptr, "open", release_page, nullptr, nullptr, SW_SHOWNORMAL);
#else
    char program[] = "xdg-open";
    char* args[] = {program, const_cast<char*>(release_page), nullptr};
    pid_t process = 0;
    if (posix_spawnp(&process, program, nullptr, nullptr, args, environ) == 0) {
        try {
            std::thread([process]() {
                int status = 0;
                while (waitpid(process, &status, 0) < 0 && errno == EINTR) {}
            }).detach();
        } catch (...) {
            // Avoid delaying application startup if a waiter cannot be created.
            waitpid(process, nullptr, WNOHANG);
        }
    }
#endif
}
}

void check_for_new_release(const char* current_version) {
    if (!current_version) return;
    CURL* curl = curl_easy_init();
    if (!curl) return;
    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, release_api);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "ItemManager/2.0.2");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, static_cast<long>(CURLPROTO_HTTPS));
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, static_cast<long>(CURLPROTO_HTTPS));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &append_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK || status != 200) return;

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (reader->parse(response.data(), response.data() + response.size(), &root, nullptr) &&
        is_newer_github_release(root, current_version)) {
        open_release_page();
    }
}
