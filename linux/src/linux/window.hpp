#pragma once
#include <SDL.h>
#include <string>

namespace LinuxUI {

class EventHandler {
public:
    virtual ~EventHandler() = default;
    virtual void on_close() = 0;
    virtual void on_resize(int width, int height) = 0;
    virtual void on_focus_lost() {}
    virtual void on_mouse_move(int x, int y) = 0;
    virtual void on_mouse_button(bool pressed, int button, int x, int y) = 0;
    virtual void on_key(bool pressed, SDL_Keycode key, SDL_Keymod modifiers) = 0;
};

// A native SDL window with a compatibility OpenGL context for the existing UI.
class Window {
public:
    Window() = default;
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    bool create(const char* title, int width, int height, bool visible, std::string& error);
    void poll(EventHandler& handler);
    void swap_buffers();
    bool make_current();
    SDL_GLContext create_shared_context();
    SDL_GLContext context() const { return context_; }
    SDL_Window* handle() const { return window_; }

private:
    SDL_Window* window_ = nullptr;
    SDL_GLContext context_ = nullptr;
    bool owns_video_ = false;
};
}
