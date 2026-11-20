#ifndef MOUSE_HPP
#define MOUSE_HPP

#include "jui/application/window.hpp"
#include "jui/layout/component.hpp"

namespace JUI
{

	enum MouseEvent {
		MOUSE_EVENT_LEFT_CLICK,
		MOUSE_EVENT_LEFT_RELEASE,
		MOUSE_EVENT_RIGHT_CLICK,
		MOUSE_EVENT_RIGHT_RELEASE,
		MOUSE_EVENT_MOVE
	};

	enum MouseCursor {
		MOUSE_CURSOR_ARROW,
		MOUSE_CURSOR_FINGER,
		MOUSE_CURSOR_HAND
	};

	class JUI_API Mouse
	{

	public:

		Mouse( Window* window );

		// Mouse control and settings.
		void set_window( Window* window );
		void poll();
		void set_client_position(int x, int y);

		// State getter.
		void set_pressed( bool is_pressed );
		bool is_pressed() const;
		HWND get_window_handle() const { return window_ ? window_->get_handle() : nullptr; }

		// Position functions.
		int get_x( void ) const;
		int get_y( void ) const;

		// Call to components.
		bool is_touching( const Component* component ) const;

	private:

		bool is_pressed_;
		bool inside_viewport_;
		Window* window_;
		POINT   position_;

	};

}

#endif // MOUSE_HPP
