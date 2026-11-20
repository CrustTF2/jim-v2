#include <jui/application/application_controller.hpp>
#include <cassert>

static int clicks=0,releases=0,right_keys=0,shift_keys=0,a_keys=0,d_keys=0,other_keys=0,focus_losses=0,frames=0;
class TestApplication : public JUI::Application {
public:
    explicit TestApplication(HINSTANCE instance):Application(instance){set_size(320,240);}
    JUI::IOResult on_mouse_clicked(JUI::Mouse*) override {++clicks;return JUI::IO_RESULT_HANDLED;}
    JUI::IOResult on_mouse_released(JUI::Mouse*) override {++releases;return JUI::IO_RESULT_HANDLED;}
    JUI::IOResult on_mouse_moved(JUI::Mouse*) override {return JUI::IO_RESULT_HANDLED;}
    JUI::IOResult on_key_pressed(int key) override {
        if(key==VK_RIGHT)++right_keys;
        if(key==VK_SHIFT)++shift_keys;
        if(key=='A')++a_keys;
        if(key=='D')++d_keys;
        if(key=='X')++other_keys;
        return JUI::IO_RESULT_HANDLED;
    }
    JUI::IOResult on_key_released(int) override {return JUI::IO_RESULT_HANDLED;}
    void on_focus_lost() override {
        ++focus_losses;
        assert(!keyboard_.is_key_pressed(VK_SHIFT));
        assert(!mouse_.is_pressed());
    }
    ReturnStatus run() override {++frames;draw_frame();swap_buffers();exit_application();return Success;}
};

int main(){
    assert(JUI::ApplicationController::initialize<TestApplication>(nullptr));
    auto app=JUI::ApplicationController::get_application();
    const auto id=SDL_GetWindowID(app->get_window()->get_handle());
    SDL_Event button{};button.type=SDL_MOUSEBUTTONDOWN;button.button.windowID=id;button.button.button=SDL_BUTTON_LEFT;
    assert(SDL_PushEvent(&button)==1);button.type=SDL_MOUSEBUTTONUP;assert(SDL_PushEvent(&button)==1);
    SDL_Event key{};key.type=SDL_KEYDOWN;key.key.windowID=id;key.key.keysym.sym=SDLK_RIGHT;assert(SDL_PushEvent(&key)==1);
    key.key.keysym.sym=SDLK_LSHIFT;assert(SDL_PushEvent(&key)==1);
    key.key.keysym.sym=SDLK_a;assert(SDL_PushEvent(&key)==1);
    key.key.repeat=1;assert(SDL_PushEvent(&key)==1);
    key.key.repeat=0;key.key.keysym.sym=SDLK_d;assert(SDL_PushEvent(&key)==1);
    key.key.repeat=1;assert(SDL_PushEvent(&key)==1);
    key.key.keysym.sym=SDLK_x;assert(SDL_PushEvent(&key)==1);
    key.key.repeat=0;key.key.keysym.sym=SDLK_RSHIFT;assert(SDL_PushEvent(&key)==1);
    SDL_Event lost{};lost.type=SDL_WINDOWEVENT;lost.window.windowID=id;
    lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;assert(SDL_PushEvent(&lost)==1);
    JUI::ApplicationController::main_loop();
    assert(clicks==1 && releases==1 && right_keys==1 && shift_keys==2 && frames==1 && focus_losses==1);
    assert(a_keys==2 && d_keys==2 && other_keys==0);
    JUI::ApplicationController::exit();
}
