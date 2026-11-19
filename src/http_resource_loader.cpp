#include "http_resource_loader.hpp"
#include <windows.h>

/*
 * Resource loader constructor.
 */
HttpResourceLoader::HttpResourceLoader( const JUTIL::String* url, JUI::FileDownloader* downloader ) :
	url_( url ),
	downloader_( downloader )
{
}

/*
 * Get a file, or retrieve it from the target.
 */
bool HttpResourceLoader::get_resource( const JUTIL::String* file, const JUTIL::String* target ) const
{
    (void)target;
    DWORD attributes=GetFileAttributesA(file->get_string());
    return attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&FILE_ATTRIBUTE_DIRECTORY);
}

/*
 * Get a file, always updating it from the url.
 */
bool HttpResourceLoader::update_resource( const JUTIL::String* file, const JUTIL::String* target ) const
{
    (void)file;(void)target;
    return false;
}

/*
 * Set file downloader.
 */
void HttpResourceLoader::set_downloader( JUI::FileDownloader* downloader )
{
	downloader_ = downloader;
}

/*
 * Set base URL for targets.
 */
void HttpResourceLoader::set_url( const JUTIL::String* url )
{
	url_ = url;
}
