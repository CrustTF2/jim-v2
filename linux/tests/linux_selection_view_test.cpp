#include "../src/item_manager_view.hpp"
#include "../src/item_manager_view_listener.hpp"
#include "../src/item_decorator.hpp"
#include "../src/notice.hpp"
#include "../src/button.hpp"
#include "../src/notification.hpp"
#include <jui/application/window.hpp>
#include <jui/application/error_stack.hpp>
#include <jui/gfx/graphics_2d.hpp>
#include <jui/io/mouse.hpp>
#include <cassert>
#include <cstdio>

struct Listener : InventoryListener, ItemManagerViewListener {
    void on_selection_changed() override {}
    bool on_error_acknowledged() override { return true; }
    void on_item_begin_move() override {}
    bool on_item_moved(Item*,unsigned int) override { return true; }
    bool on_item_end_move() override { return true; }
    bool on_craft_items() override { return true; }
    bool on_delete_item() override { return true; }
};

static void drag(ItemManagerView& view,JUI::Mouse& mouse) {
    mouse.set_client_position(1,1);
    assert(view.on_mouse_clicked(&mouse)!=JUI::IO_RESULT_ERROR);
    mouse.set_client_position(958,538);
    assert(view.on_mouse_moved(&mouse)!=JUI::IO_RESULT_ERROR);
    assert(view.on_mouse_released(&mouse)!=JUI::IO_RESULT_ERROR);
}

int main() {
    JUI::ErrorStack::get_instance();
    JUI::Window window(nullptr);window.set_size(960,540);window.set_initial_client_size(960,540);
    assert(window.create_window()==JUI::Window::Success);
    JUI::Graphics2D graphics(&window);assert(graphics.initialize()==JUI::Graphics2D::Success);
    assert(Notice::precache(&graphics) && Button::precache(&graphics));
    assert(ItemDecorator::precache(&graphics) && ItemDisplay::precache() && Notification::precache());
    Listener listener;Inventory inventory;inventory.set_listener(&listener);
    assert(inventory.get_inventory_slots()->set_size(50));
    ItemSchema schema;Item a(1,13,1,k_EItemQuality_Common,1,1,0,0),b(2,13,1,k_EItemQuality_Common,1,2,0,0);
    JUI::Texture texture;a.set_thumbnail_texture(&texture);b.set_thumbnail_texture(&texture);
    {
        ItemManagerView view(&inventory,&schema);view.set_size(960,540);view.set_listener(&listener);
        assert(view.initialize() && view.create_layout(&graphics));
        inventory.get_inventory_slots()->set_item(0,&a);inventory.get_inventory_slots()->set_item(1,&b);
        JUI::Mouse mouse(&window);

        SDL_SetModState(KMOD_NONE);
        drag(view,mouse);assert(inventory.get_selected_slots()->get_size()==0);
        assert(view.on_key_pressed(VK_SHIFT)==JUI::IO_RESULT_HANDLED);
        drag(view,mouse);assert(inventory.get_selected_slots()->get_size()==2);
        drag(view,mouse);assert(inventory.get_selected_slots()->get_size()==2);
        inventory.clear_selection();view.on_key_released(VK_SHIFT);

        SDL_SetModState(KMOD_RSHIFT);
        drag(view,mouse);assert(inventory.get_selected_slots()->get_size()==2);
        inventory.clear_selection();SDL_SetModState(KMOD_NONE);

        view.on_key_pressed(VK_SHIFT);mouse.set_client_position(1,1);
        view.on_mouse_clicked(&mouse);view.on_focus_lost();
        mouse.set_client_position(958,538);view.on_mouse_moved(&mouse);view.on_mouse_released(&mouse);
        assert(inventory.get_selected_slots()->get_size()==0);
        SDL_SetModState(KMOD_NONE);

        assert(view.on_slot_clicked(&mouse,inventory.get_inventory_slots(),0));
        view.on_key_pressed(VK_SHIFT);
        assert(view.on_slot_clicked(&mouse,inventory.get_inventory_slots(),1));
        assert(inventory.get_selected_slots()->get_size()==2);
        view.on_key_released(VK_SHIFT);inventory.clear_selection();

        view.on_key_pressed(VK_CONTROL);
        assert(view.on_slot_clicked(&mouse,inventory.get_inventory_slots(),0));
        assert(view.on_slot_clicked(&mouse,inventory.get_inventory_slots(),1));
        assert(inventory.get_selected_slots()->get_size()==2);
        view.on_key_released(VK_CONTROL);inventory.clear_selection();
    }
    std::puts("Selection view: blank-area drag, Shift state, existing selection and focus cancellation passed.");
}
