#ifdef NDEBUG
#undef NDEBUG
#endif
#include <jui/application/application.hpp>
#include <cassert>
#include <cstdio>

class TestApplication : public JUI::Application {
public:
    TestApplication() : JUI::Application(nullptr), focus_losses(0) {}
    JUI::IOResult on_mouse_clicked(JUI::Mouse*) override { return JUI::IO_RESULT_HANDLED; }
    JUI::IOResult on_mouse_released(JUI::Mouse*) override { return JUI::IO_RESULT_HANDLED; }
    JUI::IOResult on_mouse_moved(JUI::Mouse*) override { return JUI::IO_RESULT_HANDLED; }
    JUI::IOResult on_key_pressed(int) override { return JUI::IO_RESULT_HANDLED; }
    JUI::IOResult on_key_released(int) override { return JUI::IO_RESULT_HANDLED; }
    void on_focus_lost() override {
        ++focus_losses;
        assert(!keyboard_.is_key_pressed(VK_SHIFT));
        assert(!keyboard_.is_key_pressed(VK_CONTROL));
        assert(!mouse_.is_pressed());
    }
    void press_mouse() { mouse_.set_pressed(true); }
    void verify_scaled_mouse() {
        get_window()->set_size(960,540);
        get_window()->set_initial_client_size(1280,720);
        mouse_.set_client_position(640,360);
        assert(mouse_.get_x()==480 && mouse_.get_y()==270);
    }
    int focus_losses;
};

int main() {
    TestApplication app;
    app.verify_scaled_mouse();
    assert(app.trigger_key_pressed(VK_SHIFT));
    assert(app.trigger_key_pressed(VK_CONTROL));
    app.press_mouse();app.trigger_focus_lost();
    assert(app.focus_losses==1);
    std::puts("Windows modifier and mouse state reset on focus loss.");
}
