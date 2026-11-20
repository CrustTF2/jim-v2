#pragma once
#if defined(_WIN32)
#include <windows.h>
#else
#include <SDL.h>
#include <thread>
#endif
#include <chrono>
#include <cstdint>

// EnumDisplaySettings reports 0 or 1 when a display has no useful rate.
inline unsigned int highest_refresh_rate(unsigned int current, unsigned int candidate) {
    return candidate >= 10 && candidate <= 1000 && candidate > current ? candidate : current;
}

inline unsigned int active_display_refresh_rate() {
    unsigned int highest=0;
#if defined(_WIN32)
    DISPLAY_DEVICEW display={};
    display.cb=sizeof(display);
    for (DWORD i=0; EnumDisplayDevicesW(nullptr,i,&display,0); ++i) {
        if (display.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
            DEVMODEW mode={}; mode.dmSize=sizeof(mode);
            if (EnumDisplaySettingsW(display.DeviceName,ENUM_CURRENT_SETTINGS,&mode))
                highest=highest_refresh_rate(highest,mode.dmDisplayFrequency);
        }
        display={}; display.cb=sizeof(display);
    }
#else
    const int displays=SDL_GetNumVideoDisplays();
    for(int i=0;i<displays;++i){
        SDL_DisplayMode mode{};
        if(SDL_GetCurrentDisplayMode(i,&mode)==0 && mode.refresh_rate>0)
            highest=highest_refresh_rate(highest,static_cast<unsigned int>(mode.refresh_rate));
    }
#endif
    return highest ? highest : 60;
}

class FrameLimiter {
    using Clock=std::chrono::steady_clock;
#if defined(_WIN32)
    HANDLE timer_;
#endif
    unsigned int refresh_;
    Clock::time_point deadline_, check_display_;
    std::chrono::nanoseconds period() const {
        return std::chrono::nanoseconds((1000000000ULL + refresh_ - 1) / refresh_);
    }
public:
    FrameLimiter() :
#if defined(_WIN32)
        timer_(nullptr),
#endif
        refresh_(60) {
#if defined(_WIN32)
        // Windows 10+ offers a high-resolution timer; older systems use the
        // standard waitable timer and still retain a conservative frame cap.
        timer_=CreateWaitableTimerExW(nullptr,nullptr,0x00000002,TIMER_ALL_ACCESS);
        if (!timer_) timer_=CreateWaitableTimerW(nullptr,FALSE,nullptr);
#endif
    }
    ~FrameLimiter() {
#if defined(_WIN32)
        if (timer_) CloseHandle(timer_);
#endif
    }
    FrameLimiter(const FrameLimiter&)=delete;
    FrameLimiter& operator=(const FrameLimiter&)=delete;
    void reset() {
        refresh_=active_display_refresh_rate();
        auto now=Clock::now(); deadline_=now+period(); check_display_=now+std::chrono::seconds(5);
    }
    void wait() {
        auto now=Clock::now();
        if (now>=check_display_) {
            const auto rate=active_display_refresh_rate();
            check_display_=now+std::chrono::seconds(5);
            if (rate!=refresh_) { refresh_=rate; deadline_=now+period(); }
        }
        if (now<deadline_) {
#if defined(_WIN32)
            auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(deadline_-now).count();
            LARGE_INTEGER due; due.QuadPart=-((ns+99)/100);
            if (timer_ && SetWaitableTimer(timer_,&due,0,nullptr,nullptr,FALSE))
                WaitForSingleObject(timer_,INFINITE);
            else
                Sleep(static_cast<DWORD>((ns+999999)/1000000));
#else
            std::this_thread::sleep_until(deadline_);
#endif
            now=Clock::now();
        }
        deadline_+=period();
        if (deadline_<=now) deadline_=now+period();
    }
};
