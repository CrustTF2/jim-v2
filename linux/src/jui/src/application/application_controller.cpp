#include "jui/application/application_controller.hpp"
#include "jui/application/error_stack.hpp"

const JUTIL::ConstantString DEFAULT_LEAKS_OUTPUT_FILE = "memory_leaks.txt";

namespace JUI
{

	// Define static member.
	Application* ApplicationController::application_ = nullptr;

	/*
	 * Enter the main program loop.
	 */
	void ApplicationController::main_loop( void )
	{
#if !defined(_WIN32)
		struct Bridge : LinuxUI::EventHandler {
			Application* app; bool running=true;
			explicit Bridge(Application* value):app(value){}
			void on_close() override { running=false; }
			void on_resize(int,int) override {}
			void on_focus_lost() override { app->trigger_focus_lost(); }
			void on_mouse_move(int,int) override { app->trigger_mouse_moved(); }
			void on_mouse_button(bool pressed,int button,int,int) override {
				if(button==SDL_BUTTON_LEFT){if(pressed)app->trigger_mouse_clicked();else app->trigger_mouse_released();}
			}
			void on_key(bool pressed,SDL_Keycode key,SDL_Keymod) override {
				int code=-1;
				if(key==SDLK_LEFT)code=VK_LEFT;else if(key==SDLK_RIGHT)code=VK_RIGHT;
				else if(key==SDLK_RETURN||key==SDLK_KP_ENTER)code=VK_RETURN;
				else if(key==SDLK_LSHIFT||key==SDLK_RSHIFT)code=VK_SHIFT;
				else if(key==SDLK_LCTRL||key==SDLK_RCTRL)code=VK_CONTROL;
				else if(key>=SDLK_a && key<=SDLK_z)code=int(key-'a'+'A');
				else if(key>=0 && key<256)code=int(key);
				if(code>=0){if(pressed)app->trigger_key_pressed(code);else app->trigger_key_released(code);}
			}
		} bridge(application_);
		while(bridge.running){
			application_->get_window()->get_native_window()->poll(bridge);
			if(!bridge.running)break;
			if(application_->run()!=Application::Success){
				const JUTIL::String* error=ErrorStack::get_instance()->get_top_error();
				MessageBox(nullptr,error->get_string(),"Application Error",MB_OK|MB_ICONERROR);
				break;
			}
		}
#else
		// Error stack for reporting.
		ErrorStack* stack = ErrorStack::get_instance();

		// Enter main program loop.
		MSG msg;
		bool running = true;
		while (running) {
			// Check for Windows messages.
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				// Don't dispatch if quitting.
				if (msg.message == WM_QUIT) {
					running = false;
				}
				else {
					TranslateMessage( &msg );
					DispatchMessage( &msg );
				}
			}

			// Run frame.
			Application::ReturnStatus status = application_->run();
			if (status != Application::Success) {
				const JUTIL::String* error = stack->get_top_error();
				MessageBox(nullptr, error->get_string(), "Application Error", MB_OK | MB_ICONERROR );
			}
		}
#endif
	}

	/*
	 * Exit running application.
	 */
	void ApplicationController::exit()
	{
		// Kill application.
		JUTIL::BaseAllocator::destroy( application_ );
		set_application( nullptr );

		// Destroy error stack.
		ErrorStack::shut_down();

		// Write leaks to file.
#if defined(_DEBUG)
		JUTIL::AllocationManager* allocation_manager = JUTIL::AllocationManager::get_instance();
		if (allocation_manager != nullptr) {
			allocation_manager->dump_leaks( DEFAULT_LEAKS_OUTPUT_FILE.get_string() );
			JUTIL::AllocationManager::shut_down();
		}
#endif
	}

	/*
	 * Get application handle.
	 */
	Application* ApplicationController::get_application( void )
	{
		return application_;
	}

	/*
	 * Set application handle.
	 */
	void ApplicationController::set_application( Application* application )
	{
		JUTIL::JUTILBase::debug_assert( application == nullptr || application_ == nullptr );
		application_ = application;
	}

}
