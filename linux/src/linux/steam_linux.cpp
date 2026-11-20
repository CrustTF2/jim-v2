#include "../steam.hpp"
#include "steam_paths.hpp"
#include <jui/application/error_stack.hpp>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <spawn.h>
#include <sys/wait.h>
#include <vector>

extern char** environ;
extern bool (*Steam_BGetCallback)(HSteamPipe,CallbackMsg_t*,HSteamCall*);
extern void (*Steam_FreeLastCallback)(HSteamPipe);

namespace {
std::string env(const char* name){const char* value=std::getenv(name);return value?value:"";}
template<class T>T symbol(void* library,const char* name){return reinterpret_cast<T>(dlsym(library,name));}
}

bool Steam::load_linux_interfaces(){
    auto stack=JUI::ErrorStack::get_instance();
    setenv("SteamAppId","440",1);setenv("SteamGameId","440",1);
    const auto paths=LinuxSteam::discover_paths(env("HOME"),env("TF2_TF_DIR"),
                                               env("STEAM_CLIENT_LIBRARY"),env("STEAM_API_LIBRARY"));
    if(paths.client.empty()||paths.api.empty()){
        stack->log("Could not find the Linux Steam client or TF2 Steam API library.");return false;
    }
    api_dll_=dlopen(paths.api.c_str(),RTLD_NOW|RTLD_LOCAL);
    if(!api_dll_){stack->log("Could not load the Linux Steam API library.");return false;}
    auto init=symbol<bool(*)()>(api_dll_,"SteamAPI_Init");
    api_shutdown_=symbol<void(*)()>(api_dll_,"SteamAPI_Shutdown");
    auto get_pipe=symbol<int(*)()>(api_dll_,"SteamAPI_GetHSteamPipe");
    auto get_user=symbol<int(*)()>(api_dll_,"SteamAPI_GetHSteamUser");
    if(!init||!api_shutdown_||!get_pipe||!get_user){
        stack->log("The Linux Steam API library is missing required functions.");close_interfaces();return false;
    }
    if(!init()){stack->log("Could not initialize Steam. Start Steam and sign in first.");close_interfaces();return false;}
    api_initialized_=true;
    client_dll_=dlopen(paths.client.c_str(),RTLD_NOW|RTLD_LOCAL);
    if(!client_dll_){stack->log("Could not load the Linux Steam client library.");close_interfaces();return false;}
    Steam_BGetCallback=symbol<decltype(Steam_BGetCallback)>(client_dll_,"Steam_BGetCallback");
    Steam_FreeLastCallback=symbol<decltype(Steam_FreeLastCallback)>(client_dll_,"Steam_FreeLastCallback");
    auto factory=symbol<void*(*)(const char*,int*)>(client_dll_,"CreateInterface");
    steam_client_=factory?static_cast<ISteamClient008*>(factory(STEAMCLIENT_INTERFACE_VERSION_008,nullptr)):nullptr;
    pipe_=get_pipe();user_=get_user();
    if(!Steam_BGetCallback||!Steam_FreeLastCallback||!steam_client_||!pipe_||!user_){
        stack->log("The Linux Steam client interface or session is unavailable.");close_interfaces();return false;
    }
    steam_user_=reinterpret_cast<ISteamUser012*>(steam_client_->GetISteamUser(user_,pipe_,STEAMUSER_INTERFACE_VERSION_012));
    if(!steam_user_||!steam_user_->LoggedOn()){
        stack->log("Steam is not signed in or connected.");close_interfaces();return false;
    }
    game_coordinator_=static_cast<ISteamGameCoordinator001*>(steam_client_->GetISteamGenericInterface(
        user_,pipe_,STEAMGAMECOORDINATOR_INTERFACE_VERSION_001));
    if(!game_coordinator_){stack->log("TF2 Game Coordinator interface is unavailable.");close_interfaces();return false;}
    remote_storage_=reinterpret_cast<ISteamRemoteStorage004*>(steam_client_->GetISteamRemoteStorage(
        user_,pipe_,STEAMREMOTESTORAGE_INTERFACE_VERSION_004));
    steam_utils_=reinterpret_cast<ISteamUtils005*>(steam_client_->GetISteamUtils(pipe_,STEAMUTILS_INTERFACE_VERSION_005));
    steam_friends_=reinterpret_cast<ISteamFriends009*>(steam_client_->GetISteamFriends(user_,pipe_,STEAMFRIENDS_INTERFACE_VERSION_009));
    coordinator_welcomed_=false;next_hello_=std::chrono::steady_clock::now();
    return true;
}

void Steam::send_linux_hello_if_due(){
    if(!game_coordinator_||coordinator_welcomed_)return;
    auto now=std::chrono::steady_clock::now();if(now<next_hello_)return;
    const uint32 message_id=PROTOBUF_MESSAGE_FLAG|4006u;
    const uint32 packet[2]={message_id,0};
    game_coordinator_->SendMessage(message_id,packet,sizeof(packet));
    next_hello_=now+std::chrono::seconds(5);
}

void Steam::request_decal(uint64 handle){
    if(!handle||!remote_storage_||!steam_utils_||decal_requested_.count(handle))return;
    decal_requested_.insert(handle);
    std::error_code error;std::filesystem::create_directories("thumbnail_renderer/decals",error);
    if(error)return;
    char png[128];std::snprintf(png,sizeof(png),"thumbnail_renderer/decals/%016llx.png",static_cast<unsigned long long>(handle));
    if(std::filesystem::is_regular_file(png,error))return;
    const SteamAPICall_t call=remote_storage_->UGCDownload(handle);
    if(call)decal_pending_[handle]=call;
}

void Steam::poll_decals(){
    for(auto it=decal_processes_.begin();it!=decal_processes_.end();){
        int status=0;const auto result=waitpid(*it,&status,WNOHANG);
        if(result==0)++it;else it=decal_processes_.erase(it);
    }
    if(!remote_storage_||!steam_utils_)return;
    for(auto it=decal_pending_.begin();it!=decal_pending_.end();){
        bool failed=false;
        if(!steam_utils_->IsAPICallCompleted(it->second,&failed)){++it;continue;}
        const uint64 handle=it->first;AppId_t app=0;char* name=nullptr;int32 size=0;CSteamID owner;
        if(!failed && remote_storage_->GetUGCDetails(handle,&app,&name,&size,&owner) && app==440 && size>0 && size<=8*1024*1024){
            std::vector<char> bytes(size);
            if(remote_storage_->UGCRead(handle,bytes.data(),size)==size){
                char raw[128],id[32];std::snprintf(id,sizeof(id),"%016llx",static_cast<unsigned long long>(handle));
                std::snprintf(raw,sizeof(raw),"thumbnail_renderer/decals/%s.raw",id);
                if(FILE* file=std::fopen(raw,"wb")){
                    const bool ok=std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size();std::fclose(file);
                    if(ok){
                        char program[]="python3",script[]="thumbnail_renderer/decal.py";
                        char* argv[]={program,script,id,nullptr};pid_t pid=0;
                        if(posix_spawnp(&pid,program,nullptr,nullptr,argv,environ)==0)decal_processes_.insert(static_cast<int>(pid));
                    }
                }
            }
        }
        it=decal_pending_.erase(it);
    }
}
