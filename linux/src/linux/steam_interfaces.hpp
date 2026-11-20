#pragma once
#include <cstdint>

// Method ordering matches Open Steamworks ISteamClient008 and ISteamUser012.
class SteamUser012 {
public:
    virtual int GetHSteamUser() = 0;
    virtual bool LoggedOn() = 0;
};

class SteamClient008 {
public:
    virtual int CreateSteamPipe() = 0;
    virtual bool ReleaseSteamPipe(int) = 0;
    virtual int ConnectToGlobalUser(int) = 0;
    virtual int CreateLocalUser(int*, int) = 0;
    virtual void ReleaseUser(int, int) = 0;
    virtual SteamUser012* GetISteamUser(int, int, const char*) = 0;
    virtual void* GetISteamGameServer(int, int, const char*) = 0;
    virtual void SetLocalIPBinding(uint32_t, uint16_t) = 0;
    virtual void* GetISteamFriends(int, int, const char*) = 0;
    virtual void* GetISteamUtils(int, const char*) = 0;
    virtual void* GetISteamMatchmaking(int, int, const char*) = 0;
    virtual void* GetISteamMasterServerUpdater(int, int, const char*) = 0;
    virtual void* GetISteamMatchmakingServers(int, int, const char*) = 0;
    virtual void* GetISteamGenericInterface(int, int, const char*) = 0;
};
