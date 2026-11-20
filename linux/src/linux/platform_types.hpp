#pragma once
#include <SDL.h>
#include <cstdint>
#include <chrono>
#include <algorithm>
#include <codecvt>
#include <ctime>
#include <cstring>
#include <locale>
#include <string>

// Compatibility types used by the existing cross-platform UI interfaces.
using HINSTANCE = void*;
using HWND = SDL_Window*;
using HGLRC = SDL_GLContext;
using HDC = void*;
using WORD = uint16_t;
using DWORD = uint32_t;
using ULONGLONG = uint64_t;
struct POINT { int x; int y; };
struct RECT { int32_t left; int32_t top; int32_t right; int32_t bottom; };

constexpr int VK_RETURN=0x0d, VK_SHIFT=0x10, VK_CONTROL=0x11;
constexpr int VK_LEFT=0x25, VK_RIGHT=0x27, VK_LSHIFT=0xa0;
constexpr int DT_CENTER=1;
constexpr unsigned CP_UTF8=65001;
constexpr unsigned MB_ICONERROR=0, MB_OK=0;
inline int MessageBox(void*,const char* message,const char* title,unsigned) {
    return SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,title,message,nullptr);
}

inline int GetKeyState(int key) {
    const SDL_Keymod modifiers=SDL_GetModState();
    if(key==VK_LSHIFT)return (modifiers & KMOD_LSHIFT) ? 0x8000 : 0;
    if(key==VK_SHIFT)return (modifiers & KMOD_SHIFT) ? 0x8000 : 0;
    if(key==VK_CONTROL)return (modifiers & KMOD_CTRL) ? 0x8000 : 0;
    return 0;
}

inline ULONGLONG GetTickCount64() {
    return static_cast<ULONGLONG>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
inline DWORD GetTickCount() { return static_cast<DWORD>(GetTickCount64()); }
inline HWND& linux_capture_window() { static HWND window=nullptr;return window; }
inline HWND SetCapture(HWND window) {
    HWND previous=linux_capture_window();
    if(SDL_CaptureMouse(SDL_TRUE)==0)linux_capture_window()=window;
    return previous;
}
inline HWND GetCapture() { return linux_capture_window(); }
inline bool ReleaseCapture() {
    linux_capture_window()=nullptr;return SDL_CaptureMouse(SDL_FALSE)==0;
}
inline int MultiByteToWideChar(unsigned codepage,unsigned,const char* input,int length,wchar_t* output,int capacity){
    if(codepage!=CP_UTF8 || !input || length==0 || length < -1 || capacity<0)return 0;
    try {
        const size_t bytes=length<0 ? std::strlen(input)+1 : static_cast<size_t>(length);
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        const std::wstring converted=converter.from_bytes(input,input+bytes);
        if(!output)return static_cast<int>(converted.size());
        if(static_cast<size_t>(capacity)<converted.size())return 0;
        std::copy(converted.begin(),converted.end(),output);
        return static_cast<int>(converted.size());
    } catch(const std::range_error&) { return 0; }
}
inline int localtime_s(std::tm* result,const std::time_t* value){
    return result && value && localtime_r(value,result) ? 0 : 1;
}
