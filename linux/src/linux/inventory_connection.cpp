#include "inventory_connection.hpp"
#include "steam_interfaces.hpp"
#include "gc_wire.hpp"
#include <cstdlib>
#include <dlfcn.h>

namespace LinuxSteam {
class GameCoordinator001 {
public:
    virtual int SendMessage(uint32_t, const void*, uint32_t) = 0;
    virtual bool IsMessageAvailable(uint32_t*) = 0;
    virtual int RetrieveMessage(uint32_t*, void*, uint32_t, uint32_t*) = 0;
};
namespace {
template<class T> T sym(void* lib,const char* name){return reinterpret_cast<T>(dlsym(lib,name));}
void put32(uint8_t* data,uint32_t value){for(unsigned i=0;i<4;++i)data[i]=uint8_t(value>>(8*i));}
constexpr uint32_t proto=0x80000000u, hello=4006, welcome=4004, subscribed=24, check=27, refresh=28;
constexpr uint32_t maximum_message=16u*1024u*1024u;
}

InventoryConnection::~InventoryConnection(){close();}

bool InventoryConnection::open(const std::string& client_path,const std::string& api_path,std::string& error) {
    if(initialized_ || api_ || client_){error="Steam connection already opened";return false;}
    setenv("SteamAppId","440",1);
    setenv("SteamGameId","440",1);
    api_=dlopen(api_path.c_str(),RTLD_NOW|RTLD_LOCAL);
    if(!api_){error="Could not load the Linux Steam API library";return false;}
    auto init=sym<InitFn>(api_,"SteamAPI_Init");
    shutdown_=sym<VoidFn>(api_,"SteamAPI_Shutdown");
    callbacks_=sym<VoidFn>(api_,"SteamAPI_RunCallbacks");
    auto get_pipe=sym<int(*)()>(api_,"SteamAPI_GetHSteamPipe");
    auto get_user=sym<int(*)()>(api_,"SteamAPI_GetHSteamUser");
    auto user_iface=sym<void*(*)()>(api_,"SteamAPI_SteamUser_v021");
    auto user_id=sym<uint64_t(*)(void*)>(api_,"SteamAPI_ISteamUser_GetSteamID");
    auto logged_on=sym<bool(*)(void*)>(api_,"SteamAPI_ISteamUser_BLoggedOn");
    if(!init||!shutdown_||!callbacks_||!get_pipe||!get_user||!user_iface||!user_id||!logged_on){
        error="Linux Steam API is missing required interface functions";close();return false;
    }
    if(!init()){error="Steam API could not initialize; check that Steam is running";close();return false;}
    initialized_=true;
    void* user=user_iface();
    if(!user||!logged_on(user)||(owner_=user_id(user))==0){error="Steam is not signed in";close();return false;}
    client_=dlopen(client_path.c_str(),RTLD_NOW|RTLD_LOCAL);
    if(!client_){error="Could not load the Linux Steam client library";close();return false;}
    auto factory=sym<void*(*)(const char*,int*)>(client_,"CreateInterface");
    auto client=factory ? static_cast<SteamClient008*>(factory("SteamClient008",nullptr)) : nullptr;
    const int pipe=get_pipe(), user_handle=get_user();
    if(!client||!pipe||!user_handle){error="Steam client interface or session is unavailable";close();return false;}
    coordinator_=client->GetISteamGenericInterface(user_handle,pipe,"SteamGameCoordinator001");
    if(!coordinator_){error="TF2 Game Coordinator interface is unavailable";close();return false;}
    next_hello_=std::chrono::steady_clock::now();
    error.clear();return true;
}

bool InventoryConnection::poll(const InventoryHandler& on_inventory,std::string& error) {
    if(!initialized_||!coordinator_){error="Steam connection is not open";return false;}
    auto gc=static_cast<GameCoordinator001*>(coordinator_);
    auto now=std::chrono::steady_clock::now();
    if(!welcomed_ && now>=next_hello_){
        uint8_t hello_message[8]={};put32(hello_message,proto|hello);
        if(gc->SendMessage(proto|hello,hello_message,sizeof(hello_message))!=0){
            error="TF2 Game Coordinator did not accept the connection request";return false;
        }
        next_hello_=now+std::chrono::seconds(5);
    }
    callbacks_();
    uint32_t size=0;
    for(int i=0;i<128 && gc->IsMessageAvailable(&size);++i){
        if(!size||size>maximum_message){error="Invalid TF2 Game Coordinator message size";return false;}
        std::vector<uint8_t> message(size);
        uint32_t type=0,actual=0;
        if(gc->RetrieveMessage(&type,message.data(),size,&actual)!=0||actual>size){
            error="Could not retrieve a TF2 Game Coordinator message";return false;
        }
        auto id=type&~proto;
        if(id==welcome)welcomed_=true;
        if(!(type&proto)||(id!=subscribed && id!=check))continue;
        GCWire::Slice payload;
        if(!GCWire::payload({message.data(),actual},payload)){error="Invalid TF2 message header";return false;}
        if(id==check&&!refresh_sent_){
            uint64_t requested_owner=0;
            if(!GCWire::owner(payload,requested_owner)||requested_owner!=owner_)continue;
            uint8_t request[17]={};put32(request,proto|refresh);request[8]=0x09;
            for(unsigned j=0;j<8;++j)request[9+j]=uint8_t(owner_>>(8*j));
            if(gc->SendMessage(proto|refresh,request,sizeof(request))!=0){
                error="TF2 Game Coordinator did not accept the inventory refresh";return false;
            }
            refresh_sent_=true;
        }
        if(id==subscribed){
            size_t count=0;bool items=false;
            if(!GCWire::inventory_count(payload,owner_,count,items))continue;
            has_inventory_=true;
            // Preserve the original protobuf payload for the existing item parser.
            std::vector<uint8_t> copy(payload.data,payload.data+payload.size);
            on_inventory(copy,count,items);
        }
    }
    error.clear();return true;
}

void InventoryConnection::close(){
    if(initialized_ && shutdown_)shutdown_();
    initialized_=false;coordinator_=nullptr;owner_=0;
    welcomed_=false;refresh_sent_=false;has_inventory_=false;
    callbacks_=nullptr;shutdown_=nullptr;
    // Steam's runtime may retain worker threads. Keep loaded libraries until
    // process exit instead of unloading executable code beneath those threads.
    api_=nullptr;client_=nullptr;
}
}
