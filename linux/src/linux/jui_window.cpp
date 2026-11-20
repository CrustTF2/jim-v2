#include <jui/application/window.hpp>
#include <jui/application/error_stack.hpp>

namespace JUI {
namespace {
const JUTIL::ConstantString default_title("Default Application");
}

Window::Window(HINSTANCE instance):wnd_(nullptr),icon_(0),default_icon_(true),
    title_(&default_title),instance_(instance),width_(640),height_(480),
    initial_client_width_(0),initial_client_height_(0),is_fullscreen_(false),
    has_border_(true),native_(nullptr) {}

Window::~Window(){delete native_;native_=nullptr;wnd_=nullptr;}

Window::ReturnStatus Window::create_window(){
    if(native_)return DuplicateWindowFailure;
    native_=new LinuxUI::Window();
    std::string error;
    const int width=initial_client_width_>0?initial_client_width_:width_;
    const int height=initial_client_height_>0?initial_client_height_:height_;
#if defined(JIM_HEADLESS_TEST)
    const bool visible=false;
#else
    const bool visible=true;
#endif
    if(!native_->create(title_.get_string(),width,height,visible,error)){
        ErrorStack::get_instance()->log("Window: %s",error.c_str());
        delete native_;native_=nullptr;return CreateWindowFailure;
    }
    wnd_=native_->handle();
    SDL_SetWindowMinimumSize(wnd_,width_,height_);
    if(is_fullscreen_)SDL_SetWindowFullscreen(wnd_,SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_SetWindowBordered(wnd_,has_border_?SDL_TRUE:SDL_FALSE);
    return Success;
}
HWND Window::get_handle() const{return wnd_;}
const JUTIL::String* Window::get_title() const{return &title_;}
void Window::set_title(const JUTIL::String* title){title_.set_string(title);if(wnd_)SDL_SetWindowTitle(wnd_,title_.get_string());}
void Window::set_size(int width,int height){width_=width;height_=height;if(wnd_)SDL_SetWindowMinimumSize(wnd_,width,height);}
void Window::set_initial_client_size(int width,int height){initial_client_width_=width;initial_client_height_=height;}
void Window::get_client_size(int& width,int& height) const{
    if(wnd_)SDL_GetWindowSize(wnd_,&width,&height);
    else {width=initial_client_width_>0?initial_client_width_:width_;height=initial_client_height_>0?initial_client_height_:height_;}
}
Viewport Window::get_viewport() const{int w,h;get_client_size(w,h);return fit_viewport(width_,height_,w,h);}
void Window::set_fullscreen(bool value){is_fullscreen_=value;if(wnd_)SDL_SetWindowFullscreen(wnd_,value?SDL_WINDOW_FULLSCREEN_DESKTOP:0);}
void Window::set_border(bool value){has_border_=value;if(wnd_)SDL_SetWindowBordered(wnd_,value?SDL_TRUE:SDL_FALSE);}
void Window::set_icon(WORD icon){icon_=icon;default_icon_=false;}
int Window::get_width() const{return width_;}
int Window::get_height() const{return height_;}
bool Window::is_fullscreen() const{return is_fullscreen_;}
bool Window::has_border() const{return has_border_;}
bool Window::is_active(){return wnd_ && (SDL_GetWindowFlags(wnd_)&SDL_WINDOW_INPUT_FOCUS);}
bool Window::register_class(){return true;}
void Window::set_handle(HWND handle){wnd_=handle;}
void Window::set_instance(HINSTANCE instance){instance_=instance;}
HINSTANCE Window::get_instance() const{return instance_;}
}
