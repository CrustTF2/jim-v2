#include "jui/application/application.hpp"
#include "jui/application/error_stack.hpp"
#if defined(_WIN32)
#include <tlhelp32.h>
#else
#include <dirent.h>
#include <cctype>
#include <cstring>
#include <fstream>
#include <string>
#endif

namespace JUI
{

	/*
	 * Application constructor.
	 */
	Application::Application( HINSTANCE instance )
		: render_ready_(false), window_( instance ),
		graphics_( &window_ ),
		mouse_( &window_ )
	{
		// Nothing specific.
	}

	/*
	 * Application destructor.
	 */
	Application::~Application( void )
	{
		clean_up();
	}

	/*
	 * Initialize application interfaces.
	 */
	Application::ReturnStatus Application::initialize( void )
	{
		// Set window size and create.
		window_.set_size( get_width(), get_height() );
		Window::ReturnStatus window_status = window_.create_window();
		if (window_status != Window::Success) {
			ErrorStack::get_instance()->log( "Application: failed to create window." );
			return WindowCreateFailure;
		}

		// Initialize graphics.
		Graphics2D::ReturnStatus graphics_status = graphics_.initialize();
		if (graphics_status != Graphics2D::Success) {
			ErrorStack::get_instance()->log( "Application: failed to initialize graphics." );
			return GraphicsInitializeFailure;
		}

		return Success;
	}

	/*
	 * Clean application interfaces.
	 */
	void Application::clean_up( void )
	{
		// Nothing specific.
	}

	/*
	 * Post the window destroy message.
	 */
	void Application::exit_application( void )
	{
#if defined(_WIN32)
		PostQuitMessage( 0 );
#else
		SDL_Event event{};event.type=SDL_QUIT;SDL_PushEvent(&event);
#endif
	}

	/*
	 * Return the number of processes with the given name.
	 */
	unsigned int Application::get_process_count( const char* process_name )
	{
#if defined(_WIN32)
		unsigned int count = 0;

		// Find all processes with name process_name.
		PROCESSENTRY32 entry;
		entry.dwSize = sizeof( PROCESSENTRY32 );
		HANDLE snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
		if (Process32First( snapshot, &entry )) {
			// Search for process name.
			while (Process32Next( snapshot, &entry )) {
				if (strcmp( entry.szExeFile, process_name ) == 0) {
					++count;
				}
			}
		}

		return count;
#else
		if(!process_name)return 0;
		DIR* directory=opendir("/proc");if(!directory)return 0;
		unsigned int count=0;
		while(auto entry=readdir(directory)){
			if(!std::isdigit(static_cast<unsigned char>(entry->d_name[0])))continue;
			std::ifstream file(std::string("/proc/")+entry->d_name+"/comm");
			std::string name;
			if(std::getline(file,name) && name==process_name)++count;
		}
		closedir(directory);return count;
#endif
	}

	/*
	 * Get the application's window.
	 */
	Window* Application::get_window( void )
	{
		return &window_;
	}

	Application::ReturnStatus Application::run( void )
	{
		// Do nothing.
		if (!trigger_mouse_moved()) {
			return MouseHandlingFailure;
		}

		// No issues.
		return Success;
	}

	/*
	 * Run the application's draw routine.
	 */
	void Application::draw_frame( void )
	{
		graphics_.setup_window_scene();
		graphics_.clear_scene();
		draw( &graphics_ );
	}

	/*
	 * Swap the buffer and show what we drew.
	 */
	void Application::swap_buffers( void )
	{
		graphics_.swap_buffers();
	}

	/*
	 * Updates the mouse position and triggers event.
	 * Returns true if mouse handling successful, false otherwise.
	 */
	bool Application::trigger_mouse_moved( void )
	{
		// Update mouse regardless of focus.
		mouse_.poll();
		IOResult result = on_mouse_moved( &mouse_ );
		if (result == IO_RESULT_ERROR) {
			return false;
		}

		// No issues.
		return true;
	}

	/*
	 * Triggers mouse click.
	 * Returns true if mouse handling successful, false otherwise.
	 */
	bool Application::trigger_mouse_clicked( void )
	{
		mouse_.set_pressed( true );
		mouse_.poll();
		IOResult result = on_mouse_clicked( &mouse_ );
		if (result == IO_RESULT_ERROR) {
			return false;
		}

		// No issues.
		return true;
	}

	/*
	 * Triggers mouse released.
	 * Returns true if mouse handling successful, false otherwise.
	 */
	bool Application::trigger_mouse_released( void )
	{
		mouse_.set_pressed( false );
		mouse_.poll();
		IOResult result = on_mouse_released( &mouse_ );
		if (result == IO_RESULT_ERROR) {
			return false;
		}

		// No issues.
		return true;
	}

	/*
	 * Update key pressed state and trigger event.
	 */
	bool Application::trigger_key_pressed( int key )
	{
		// Update state.
		keyboard_.set_key_state( key, true );
		IOResult result = on_key_pressed( key );
		if (result == IO_RESULT_ERROR) {
			return false;
		}

		// No issue.
		return true;
	}

	/*
	 * Update key released state and trigger event.
	 */
	void Application::trigger_focus_lost()
	{
		keyboard_.clear_states();
		mouse_.set_pressed(false);
		on_focus_lost();
	}

	bool Application::trigger_key_released( int key )
	{
		// Update state.
		keyboard_.set_key_state( key, false );
		IOResult result = on_key_released( key );
		if (result == IO_RESULT_ERROR) {
			return false;
		}

		// No issue.
		return true;
	}

}
