#pragma once
#include <windows.h>
#include <chrono>
#include <cstdint>

// EnumDisplaySettings reports 0 or 1 when a display has no useful rate.
inline unsigned int highest_refresh_rate(unsigned int current, unsigned int candidate) {
    return candidate >= 10 && candidate <= 1000 && candidate > current ? candidate : current;
}

inline unsigned int active_display_refresh_rate() {
    unsigned int highest=0;
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
    return highest ? highest : 60;
}

class FrameLimiter {
    using Clock=std::chrono::steady_clock;
    HANDLE timer_;
    unsigned int refresh_;
    Clock::time_point deadline_, check_display_;
    std::chrono::nanoseconds period() const {
        return std::chrono::nanoseconds((1000000000ULL + refresh_ - 1) / refresh_);
    }
public:
    FrameLimiter() : timer_(nullptr), refresh_(60) {
        // Windows 10+ offers a high-resolution timer; older systems use the
        // standard waitable timer and still retain a conservative frame cap.
        timer_=CreateWaitableTimerExW(nullptr,nullptr,0x00000002,TIMER_ALL_ACCESS);
        if (!timer_) timer_=CreateWaitableTimerW(nullptr,FALSE,nullptr);
    }
    ~FrameLimiter() { if (timer_) CloseHandle(timer_); }
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
            auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(deadline_-now).count();
            LARGE_INTEGER due; due.QuadPart=-((ns+99)/100);
            if (timer_ && SetWaitableTimer(timer_,&due,0,nullptr,nullptr,FALSE))
                WaitForSingleObject(timer_,INFINITE);
            else
                Sleep(static_cast<DWORD>((ns+999999)/1000000));
            now=Clock::now();
        }
        deadline_+=period();
        if (deadline_<=now) deadline_=now+period();
    }
};
