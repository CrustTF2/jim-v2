#include "string/dynamic_wide_string.hpp"
#include "memory/base_allocator.hpp"
#include <stdio.h>
#include <stdarg.h>
#include <vector>
#include <cwchar>

namespace JUTIL
{

	/*
	 * String constructor from string builder.
	 * Releases the buffer from the builder and holds it in the string object.
	 */
	DynamicWideString::DynamicWideString( void )
		: BaseDynamicString<wchar_t>()
	{
	}

	/*
	 * String constructor from string and length.
	 * Does not assume string is null-terminated; does not terminate it itself.
	 */
	DynamicWideString::DynamicWideString( wchar_t* string, size_t length )
		: BaseDynamicString<wchar_t>( string, length )
	{
	}

	/*
	 * String destructor (already cleared by base dynamic).
	 */
	DynamicWideString::~DynamicWideString( void )
	{
	}

	/*
	 * Write formatted string output to the buffer.
	 * Returns a pointer to the new string if succeeded, nullptr otherwise.
	 */
	bool DynamicWideString::write( const wchar_t* format, ... )
	{
		// Get needed size.
		va_list args;
		va_start( args, format );
		bool ok = write( format, args );
		va_end( args );
		return ok;
	}

	/*
	 * Write formatted string output to buffer with variable arguments.
	 * Returns a pointer to the new string if succeeded, nullptr otherwise.
	 * Assumes 'args' list has been started.
	 */
	bool DynamicWideString::write( const wchar_t* format, va_list args )
	{
		std::vector<wchar_t> buffer(128);
		for (; buffer.size() <= 1024 * 1024; buffer.resize(buffer.size() * 2)) {
			va_list copy;
			va_copy(copy, args);
			int written = vswprintf(buffer.data(), buffer.size(), format, copy);
			va_end(copy);
			if (written < 0 || static_cast<size_t>(written) >= buffer.size()) continue;
			size_t old_length = get_length();
			if (!set_length(old_length + static_cast<size_t>(written))) return false;
			std::wmemcpy(builder_.get_array() + old_length, buffer.data(), static_cast<size_t>(written) + 1);
			return true;
		}
		return false;
	}

}
