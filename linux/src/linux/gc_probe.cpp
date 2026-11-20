// Read-only TF2 Game Coordinator inventory-cache probe.
#include "steam_interfaces.hpp"
#include "gc_wire.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <sys/resource.h>
#include <thread>
#include <vector>

class GameCoordinator001 {
public:
    virtual int SendMessage(uint32_t, const void*, uint32_t) = 0;
    virtual bool IsMessageAvailable(uint32_t*) = 0;
    virtual int RetrieveMessage(uint32_t*, void*, uint32_t, uint32_t*) = 0;
};

static void result(const char* key, const char* value) {
    std::printf("%s=%s\n", key, value);
    std::fflush(stdout);
}
static void number(const char* key, long long value) {
    std::printf("%s=%lld\n", key, value);
    std::fflush(stdout);
}
template<class T> static T symbol(void* lib, const char* name) {
    return reinterpret_cast<T>(dlsym(lib, name));
}
static void put32(uint8_t* data, uint32_t value) {
    for (unsigned i=0;i<4;i++) data[i]=uint8_t(value >> (8*i));
}

int main(int argc, char** argv) {
    if (argc!=3) { result("error", "expected Steam client and Steam API paths"); return 2; }
    const struct rlimit no_core={0,0}; setrlimit(RLIMIT_CORE,&no_core);
    setenv("SteamAppId", "440", 1);
    setenv("SteamGameId", "440", 1);
    result("stage", "loading native Steam API");
    void* api=dlopen(argv[2],RTLD_NOW | RTLD_LOCAL);
    if (!api) { result("api_loaded","no"); result("loader_error",dlerror()); return 1; }
    result("api_loaded","yes");
    auto init=symbol<bool(*)()>(api,"SteamAPI_Init");
    auto shutdown=symbol<void(*)()>(api,"SteamAPI_Shutdown");
    auto callbacks=symbol<void(*)()>(api,"SteamAPI_RunCallbacks");
    auto get_pipe=symbol<int(*)()>(api,"SteamAPI_GetHSteamPipe");
    auto get_user=symbol<int(*)()>(api,"SteamAPI_GetHSteamUser");
    auto steam_user=symbol<void*(*)()>(api,"SteamAPI_SteamUser_v021");
    auto user_id=symbol<uint64_t(*)(void*)>(api,"SteamAPI_ISteamUser_GetSteamID");
    auto logged_on=symbol<bool(*)(void*)>(api,"SteamAPI_ISteamUser_BLoggedOn");
    if (!init || !shutdown || !callbacks || !get_pipe || !get_user || !steam_user || !user_id || !logged_on) {
        result("api_exports","missing required Steam API functions"); return 1;
    }
    result("api_exports","available");
    result("stage","initializing Steam API for TF2");
    if (!init()) { result("api_initialized","no"); return 1; }
    result("api_initialized","yes");
    struct ApiScope { void(*shutdown)(); ~ApiScope(){shutdown();} } api_scope{shutdown};
    void* user_interface=steam_user();
    if (!user_interface || !logged_on(user_interface)) { result("api_login","not logged on"); return 1; }
    result("api_login","logged on");
    const uint64_t owner_id=user_id(user_interface);
    if (!owner_id) { result("account_identity","unavailable"); return 1; }
    result("account_identity","available (not shown)");

    void* lib=dlopen(argv[1],RTLD_NOW | RTLD_LOCAL);
    if (!lib) { result("client_loaded","no"); result("loader_error",dlerror()); return 1; }
    result("client_loaded","yes");
    using Factory=void*(*)(const char*,int*);
    auto factory=symbol<Factory>(lib,"CreateInterface");
    auto client=factory ? static_cast<SteamClient008*>(factory("SteamClient008",nullptr)) : nullptr;
    if (!client) { result("steamclient008","unavailable"); return 1; }
    const int pipe=get_pipe(), user=get_user();
    if (!pipe || !user) { result("api_context","unavailable"); return 1; }
    auto gc=static_cast<GameCoordinator001*>(client->GetISteamGenericInterface(user,pipe,"SteamGameCoordinator001"));
    if (!gc) { result("game_coordinator_interface","unavailable"); return 1; }
    result("game_coordinator_interface","available");
    result("stage","requesting read-only inventory cache");

    constexpr uint32_t proto=0x80000000u, hello=4006, welcome=4004, subscribed=24, check=27, refresh=28;
    constexpr uint32_t max_message=16u*1024u*1024u;
    auto start=std::chrono::steady_clock::now(), next_hello=start;
    bool got_cache=false, sent_refresh=false, saw_welcome=false;
    int received=0;
    while (std::chrono::steady_clock::now()-start < std::chrono::seconds(30)) {
        auto now=std::chrono::steady_clock::now();
        if (now>=next_hello && !saw_welcome) {
            uint8_t bytes[8]={}; put32(bytes,proto|hello);
            number("hello_send_result",gc->SendMessage(proto|hello,bytes,sizeof(bytes)));
            next_hello=now+std::chrono::seconds(5);
        }
        callbacks();
        uint32_t size=0;
        for (int i=0;i<128 && gc->IsMessageAvailable(&size);++i) {
            if (!size || size>max_message) { result("gc_error","message has invalid or excessive size"); return 1; }
            std::vector<uint8_t> bytes(size);
            uint32_t type=0, actual=0;
            if (gc->RetrieveMessage(&type,bytes.data(),size,&actual)!=0 || actual>size) {
                result("gc_error","could not retrieve a coordinator message"); return 1;
            }
            ++received;
            uint32_t id=type & ~proto;
            if (id==welcome) { saw_welcome=true; result("gc_welcome","received"); }
            if (!(type & proto) || (id!=subscribed && id!=check)) continue;
            GCWire::Slice payload;
            if (!GCWire::payload({bytes.data(),actual},payload)) { result("gc_error","invalid message header"); return 1; }
            if (id==check && !sent_refresh) {
                uint64_t requested_owner=0;
                if (!GCWire::owner(payload,requested_owner) || requested_owner!=owner_id) continue;
                uint8_t request[17]={}; put32(request,proto|refresh); request[8]=0x09;
                for (unsigned j=0;j<8;++j) request[9+j]=uint8_t(owner_id >> (8*j));
                int code=gc->SendMessage(proto|refresh,request,sizeof(request));
                number("cache_refresh_result",code);
                sent_refresh=(code==0);
            }
            if (id==subscribed) {
                size_t count=0; bool has_items=false;
                if (!GCWire::inventory_count(payload,owner_id,count,has_items)) continue;
                result("inventory_cache","received for active account");
                result("inventory_item_group",has_items?"present":"not present");
                if (has_items) number("inventory_item_count",static_cast<long long>(count));
                got_cache=true;
                break;
            }
        }
        if (got_cache) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    number("gc_messages_received",received);
    if (!got_cache) result("inventory_cache","not received within 30 seconds");
    result("stage","shutting down temporary Steam API session");
    return got_cache ? 0 : 1;
}
