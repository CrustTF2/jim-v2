#include "jui/io/keyboard.hpp"

namespace JUI
{

	/*
	 * Keyboard contructor.
	 */
	Keyboard::Keyboard( void )
	{
		clear_states();
	}

	/*
	 * Returns true if a key is pressed.
	 */
	bool Keyboard::is_key_pressed( int key_code ) const
	{
		return key_code >= 0 && static_cast<size_t>(key_code) < KEY_COUNT && states_[key_code];
	}

	/*
	 * Updates a key's state.
	 */
	void Keyboard::set_key_state( int key, bool is_pressed )
	{
		if (key >= 0 && static_cast<size_t>(key) < KEY_COUNT) states_[key] = is_pressed;
	}

	/*
	 * Resets all key states.
	 */
	void Keyboard::clear_states( void )
	{
		for (size_t i = 0; i < KEY_COUNT; ++i) {
			states_[i] = false;
		}
	}

}
