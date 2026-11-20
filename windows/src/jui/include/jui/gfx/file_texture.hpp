#ifndef FILE_TEXTURE_HPP
#define FILE_TEXTURE_HPP

#include "jui/gfx/texture.hpp"
#include <string/string.hpp>
#include <string/dynamic_string.hpp>

namespace JUI
{

	class JUI_API FileTexture : public Texture
	{
		
	public:

		FileTexture( const JUTIL::String* filename );
		const JUTIL::String* get_filename( void ) const;

	private:

		JUTIL::DynamicString filename_;

	};

}

#endif // FILE_TEXTURE_HPP
