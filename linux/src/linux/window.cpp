#include "window.hpp"

namespace LinuxUI {

Window::~Window() {
    if (context_) SDL_GL_DeleteContext(context_);
    if (window_) SDL_DestroyWindow(window_);
    if (owns_video_) SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

bool Window::create(const char* title, int width, int height, bool visible, std::string& error) {
    if (window_ || width <= 0 || height <= 0) {
        error = "Invalid window size or window already created";
        return false;
    }
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        error = SDL_GetError();
        return false;
    }
    owns_video_ = true;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    const auto flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
        (visible ? SDL_WINDOW_SHOWN : SDL_WINDOW_HIDDEN);
    window_ = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               width, height, flags);
    if (!window_) { error = SDL_GetError(); return false; }
    context_ = SDL_GL_CreateContext(window_);
    if (!context_) { error = SDL_GetError(); return false; }
    SDL_GL_SetSwapInterval(1);
    error.clear();
    return true;
}

void Window::poll(EventHandler& handler) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) handler.on_close();
        else if (event.type == SDL_WINDOWEVENT && event.window.windowID == SDL_GetWindowID(window_)) {
            if (event.window.event == SDL_WINDOWEVENT_CLOSE) handler.on_close();
            else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                handler.on_resize(event.window.data1, event.window.data2);
            else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                handler.on_focus_lost();
        } else if (event.type == SDL_MOUSEMOTION && event.motion.windowID == SDL_GetWindowID(window_))
            handler.on_mouse_move(event.motion.x, event.motion.y);
        else if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) &&
                 event.button.windowID == SDL_GetWindowID(window_))
            handler.on_mouse_button(event.type == SDL_MOUSEBUTTONDOWN,
                                    event.button.button, event.button.x, event.button.y);
        else if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) &&
                 event.key.windowID == SDL_GetWindowID(window_)) {
            const auto key=event.key.keysym.sym;
            // Page navigation uses repeated key-down events, as on Windows.
            const bool page_key=key==SDLK_a || key==SDLK_d || key==SDLK_LEFT || key==SDLK_RIGHT;
            if (!event.key.repeat || (event.type==SDL_KEYDOWN && page_key))
                handler.on_key(event.type == SDL_KEYDOWN, key,
                               static_cast<SDL_Keymod>(event.key.keysym.mod));
        }
    }
}

void Window::swap_buffers() { if (window_ && context_) SDL_GL_SwapWindow(window_); }
bool Window::make_current() { return window_ && context_ && SDL_GL_MakeCurrent(window_, context_) == 0; }

SDL_GLContext Window::create_shared_context() {
    if (!make_current() || SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1) != 0) return nullptr;
    SDL_GLContext shared = SDL_GL_CreateContext(window_);
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
    if (shared) make_current();
    return shared;
}
}
