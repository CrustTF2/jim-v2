#include "updater.hpp"
#include <windows.h>
#include <json/json.h>
#include <jui/application/error_stack.hpp>
#include <jui/jui_version.hpp>
#include <jutil_version.hpp>
#include "github_release.hpp"
#include <bcrypt.h>
#include <fstream>
#include <vector>
#include <memory>

// Version checking parameters.
const JUTIL::ConstantString VERSIONS_URL( "https://api.github.com/repos/CrustTF2/jim-v2/releases/latest" );

bool verify_update_file(const char* path,const std::string& expected)
{
 std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)return false;
 auto length=file.tellg();if(length<512||length>64ll*1024*1024)return false;
 file.seekg(0);unsigned char header[64];file.read(reinterpret_cast<char*>(header),sizeof(header));
 if(!file||header[0]!='M'||header[1]!='Z')return false;
 uint32_t pe_offset=uint32_t(header[60])|uint32_t(header[61])<<8|uint32_t(header[62])<<16|uint32_t(header[63])<<24;
 if(uint64_t(pe_offset)+6>uint64_t(length))return false;
 unsigned char pe[6];file.seekg(pe_offset);file.read(reinterpret_cast<char*>(pe),6);
 if(!file||pe[0]!='P'||pe[1]!='E'||pe[2]||pe[3]||pe[4]!=0x4c||pe[5]!=0x01)return false;
 file.seekg(0);BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
 DWORD hash_size=0,object_size=0,returned=0;bool ok=false;
 if(BCryptGetProperty(algorithm,BCRYPT_HASH_LENGTH,reinterpret_cast<PUCHAR>(&hash_size),sizeof(hash_size),&returned,0)>=0
  && BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&object_size),sizeof(object_size),&returned,0)>=0
  && hash_size==32 && object_size>0 && object_size<1024*1024){
  std::vector<unsigned char> object(object_size),digest(hash_size),buffer(65536);
  if(BCryptCreateHash(algorithm,&hash,object.data(),object_size,nullptr,0,0)>=0){
   ok=true;while(file){file.read(reinterpret_cast<char*>(buffer.data()),buffer.size());auto count=file.gcount();if(count>0&&BCryptHashData(hash,buffer.data(),static_cast<ULONG>(count),0)<0){ok=false;break;}}
   if(!file.eof()||BCryptFinishHash(hash,digest.data(),hash_size,0)<0)ok=false;
   static const char* hex="0123456789abcdef";std::string actual;for(auto b:digest){actual+=hex[b>>4];actual+=hex[b&15];}
   ok=ok && actual==expected;
  }
 }
 if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);return ok;
}

// File removal parameters.
const long REMOVE_WAIT_INTERVAL = 250;
const unsigned int REMOVE_RETRY_MAXIMUM = 8;

// List of files to remove/update.
const JUTIL::ConstantString JIM_FILE( "item_manager.exe" );
const JUTIL::ConstantString UPDATE_FILES[] = {
	JIM_FILE
};

/*
 * Just set the downloader object we're using to get the files.
 */
Updater::Updater( JUI::FileDownloader* downloader, HttpResourceLoader* site_loader,	const JUTIL::String* jim_version ) :
	downloader_( downloader ),
	site_loader_( site_loader ),
	jim_version_( jim_version ),
	status_( UPDATE_PENDING ),
	is_jim_latest_( false )
{
}

/*
 * Clean up.
 */
Updater::~Updater( void )
{
}

/*
 * Clean up the old version of the files.
 */
bool Updater::remove_old_files( void )
{
	// Stack for reporting errors.
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

	// Look for old files.
	for (const JUTIL::ConstantString& update_file : UPDATE_FILES) {
		JUTIL::DynamicString old_filename;
		if (!old_filename.write( "%s.old", update_file.get_string() )) {
			stack->log( "Failed to write old file name to buffer." );
			return false;
		}

		// Keep trying to delete the file if it exists.
		FILE* file;
		errno_t error = fopen_s( &file, old_filename.get_string(), "r" );
		if (file != nullptr) {
			fclose( file );

			// Try to delete the file now.
			unsigned int j;
			bool removed_file = false;
			for (j = 0; j < REMOVE_RETRY_MAXIMUM; ++j) {
				if (DeleteFile( old_filename.get_string() )) {
					removed_file = true;
					break;
				}
				Sleep( REMOVE_WAIT_INTERVAL );
			}

			// Did we get rid of it?
			if (!removed_file) {
				stack->log( "Took too long to get rid of %s.", old_filename.get_string() );
				return false;
			}
		}
	}

	return true;
}

/*
 * Get version numbers and update component state.
 * Only returns an update being required if we successfully retrieve and parse version information.
 */
UpdateCheckResult Updater::check_version_numbers( void )
{
	// Stack for error reporting.
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

	// Read latest version information.
	JUTIL::DynamicString versions;
	downloader_->set_timeout(10);
	const bool downloaded=downloader_->read( &VERSIONS_URL, &versions );
	downloader_->clear_timeout();
	if (!downloaded) {
		// If server goes down, don't bother complaining.
		return UP_TO_DATE;
	}

	// Parse the JSON.
	Json::Value root;
	Json::CharReaderBuilder builder;
	std::unique_ptr<Json::CharReader> reader( builder.newCharReader() );
	const char* versions_start = versions.get_string();
	const char* versions_end = versions_start + versions.get_length();
	if (!reader->parse( versions_start, versions_end, &root, nullptr )) {
		return UP_TO_DATE;
	}

	// Get version numbers.
	GithubRelease release;
	if (!select_github_release(root,jim_version_->get_string(),release)) return UP_TO_DATE;
	release_url_=release.url;release_sha256_=release.sha256;is_jim_latest_=false;
	return UPDATE_REQUIRED;
}

/*
 * Update all files that are out of date.
 */
bool Updater::update( void )
{
	try {
		if (!is_jim_latest_ && !update_file( &JIM_FILE )) {
			status_=UPDATE_FAILED;
			return false;
		}
		status_ = UPDATE_SUCCESSFUL;
		return true;
	} catch (...) {
		status_=UPDATE_FAILED;
		return false;
	}
}

/*
 * Run the new version of the item manager.
 */
bool Updater::run_new_version( void )
{
	// Error stack for logging.
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

	// Create structs.
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory( &si, sizeof(si) );
	si.cb = sizeof(si);
	ZeroMemory( &pi, sizeof(pi) );

	// Create copy of file name.
	JUTIL::DynamicString file;
	if (!file.copy( &JIM_FILE )) {
		return false;
	}
	if (!CreateProcess( nullptr, file.get_string(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si,	&pi)) {
		DeleteFileA(file.get_string());
		MoveFileExA("item_manager.exe.old",file.get_string(),MOVEFILE_WRITE_THROUGH);
		return false;
	}
	CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
	return true;
}

/*
 * Get update status.
 */
UpdateStatus Updater::get_update_status( void )
{
	return status_.load();
}

/*
 * Compare version and perform update on file if out of date.
 */
bool Updater::update_file( const JUTIL::String* file )
{
	// Stack for reporting errors.
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

	// Get name for new file.
	JUTIL::DynamicString new_name;
	if (!new_name.write( "%s.new", file->get_string() )) {
		status_ = UPDATE_FAILED;
		return false;
	}

	// Save the new version before moving the old one.
	const JUTIL::ConstantString source(release_url_.c_str());
	downloader_->set_timeout(120);
	const bool downloaded=downloader_->get( &new_name, &source );
	downloader_->clear_timeout();
	if (!downloaded || !verify_update_file(new_name.get_string(),release_sha256_)) {
		DeleteFileA(new_name.get_string());
		status_ = UPDATE_FAILED;
		return false;
	}

	// Need to move current file to .old suffix.
	JUTIL::DynamicString moved_name;
	if (!moved_name.write( "%s.old", file->get_string() )) {
		stack->log( "Failed to write new file name for updated file." );
		status_ = UPDATE_FAILED;
		return false;
	}

	// Check if current file even exists; user may have renamed EXE, for instance.
	FILE* moved;
	errno_t err = fopen_s( &moved, file->get_string(), "r" );
	if (moved != nullptr) {
		fclose( moved );

		// File is there, now move it.
		if (!MoveFile( file->get_string(), moved_name.get_string() )) {
			stack->log( "Failed to move updated file." );
			status_ = UPDATE_FAILED;
			return false;
		}
	}

	// Now move the new one to the original spot.
	if (!MoveFile( new_name.get_string(), file->get_string() )) {
		MoveFileExA(moved_name.get_string(),file->get_string(),MOVEFILE_WRITE_THROUGH);
		stack->log( "Failed to rename updated file to original name." );
		status_ = UPDATE_FAILED;
		return false;
	}
	return true;
}
