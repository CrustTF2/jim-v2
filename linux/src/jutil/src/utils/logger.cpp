#include "utils/logger.hpp"
#include <stdarg.h>

namespace JUTIL
{

	/*
	 * Logger constructor.
	 */
	Logger::Logger( void ) :
		file_( nullptr )
	{
	}

	/*
	 * Logger destructor.
	 */
	Logger::~Logger( void )
	{
		// Close file if open.
		if (file_ != nullptr) {
			close();
		}
	}

	/*
	 * Open log to file.
	 */
	bool Logger::open( const ConstantString& filename )
	{
		// Attempt to open file.
		file_ = fopen( filename.get_string(), "w" );
		if (!file_) {
			return false;
		}

		// Create initial message.
		return write( "Logging started...\n" );
	}

	/*
	 * Close file.
	 */
	void Logger::close( void )
	{
		fclose( file_ );
	}

	/*
	 * Write text to file.
	 */
	bool Logger::write( const char* format, ... )
	{
		// Write to buffer.
		va_list args;
		va_start( args, format );
		int written = vfprintf( file_, format, args );
		va_end( args );
		if (written < 0) {
			return false;
		}

		// Flush buffer to file.
		return fflush( file_ ) == 0;
	}

}
