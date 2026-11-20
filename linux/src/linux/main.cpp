#include "../item_manager.hpp"
#include <jui/application/application_controller.hpp>

int main() {
    const bool started=JUI::ApplicationController::initialize<ItemManager>(nullptr);
    if (started) JUI::ApplicationController::main_loop();
    JUI::ApplicationController::exit();
    return started ? 0 : 1;
}
