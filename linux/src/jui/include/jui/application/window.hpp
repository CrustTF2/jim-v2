#ifndef WINDOW_HPP
#define WINDOW_HPP

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include "linux/platform_types.hpp"
#include "linux/window.hpp"
#endif
#include <string/string.hpp>
#include "jui/jui_base.hpp"
#include "jui/application/viewport.hpp"

namespace JUI
{

	// Prototype for window handling.
#if defined(_WIN32)
	LRESULT CALLBACK wnd_proc( HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam );
#endif

	class JUI_API Window
	{

	public:

		// Window return status.
		enum ReturnStatus
		{
			Success = 0,            // No error.
			RegisterClassFailure,   // Failed to register window class.
			DuplicateWindowFailure, // Window already exists.
			AdjustSizeFailure,      // Failed to adjust window rect.
			CreateWindowFailure,    // Failed to create window.
		};

	public:

		//Class constructor.
		Window( HINSTANCE hInstance );
		~Window( void );

		// Window creation.
		ReturnStatus create_window( void );
		
		// Get window external parameters.
		HWND get_handle( void ) const;
		const JUTIL::String* get_title( void ) const;

		// Setting window display parameters.
		void set_title( const JUTIL::String* title );
		void set_size( int width, int height );
		void set_initial_client_size(int width, int height);
		Viewport get_viewport() const;
		void get_client_size(int& width, int& height) const;
		void set_fullscreen( bool is_fullscreen );
		void set_border( bool has_border );
		void set_icon( WORD icon );

		// Get window display parameters.
		int get_width( void ) const;
		int get_height( void ) const;
		bool is_fullscreen( void ) const;
		bool has_border( void ) const;

		// Window status parameters.
		bool is_active( void );

#if !defined(_WIN32)
		LinuxUI::Window* get_native_window() const { return native_; }
#endif

	private:

		// Creating window class.
		bool register_class( void );

		// Setting window external parameters.
		void set_handle( HWND hwnd );
		void set_instance( HINSTANCE instance );
		HINSTANCE get_instance( void ) const;

	private:

		// Window handle.
		HWND wnd_;

		// Icon data
		WORD icon_;
		bool default_icon_;

		// Window external parameters.
		JUTIL::ConstantString title_;
		HINSTANCE instance_;

		// Window display parameters.
		int width_;
		int height_;
		int initial_client_width_;
		int initial_client_height_;
		bool is_fullscreen_;
		bool has_border_;

#if !defined(_WIN32)
		LinuxUI::Window* native_;
#endif

	};

}

#endif // WINDOW_HPP
