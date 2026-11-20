#include "steam.hpp"
#include "serialized_buffer.hpp"
#include "protobuf/base_gcmessages.pb.h"
#include "protobuf/gcsdk_gcmessages.pb.h"
#include <string/string.hpp>
#include <jui/application/error_stack.hpp>

/* Function prototypes to load from DLL. */
bool (*Steam_BGetCallback) (HSteamPipe hSteamPipe, CallbackMsg_t *pCallbackMsg, HSteamCall *phSteamCall);
void (*Steam_FreeLastCallback) (HSteamPipe hSteamPipe);

// File for logging messages sent and received.
const JUTIL::ConstantString STEAM_LOG_FILE( "steam_messages.txt" );

Steam::Steam( void ) :
	user_( 0 ),
	pipe_( 0 ),
	steam_client_( nullptr ),
	steam_user_( nullptr ),
	game_coordinator_( nullptr ),
	remote_storage_( nullptr ),
	steam_utils_( nullptr ),
	steam_friends_( nullptr ),
	client_dll_( nullptr )
{
#if defined(LOG_STEAM_MESSAGES)

	// Try to open file for logging.
	fopen_s( &log_, STEAM_LOG_FILE.get_string(), "w" );

#endif
}

Steam::~Steam( void )
{
	//Steam has been destroyed.
	close_interfaces();

#if defined(LOG_STEAM_MESSAGES)

	// Close log file if open.
	if (log_ != nullptr) {
		fclose( log_ );
	}

#endif
}

/*
 * Load Steam interfaces.
 */
bool Steam::load_interfaces( void )
{
#if !defined(_WIN32)
	return load_linux_interfaces();
#else
    // Get error stack for reporting.
    JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

	// Set Team Fortress 2 application ID.
	SetEnvironmentVariable( "SteamAppId", "440" );
    JUTIL::ArrayBuilder<char> install_path;
	
	// Initial allocation.
	const size_t DEFAULT_PATH_SIZE = 32;
	if (!install_path.set_size( DEFAULT_PATH_SIZE )) {
		stack->log( "Failed to allocate initial string for Steam install path registry value." );
		return false;
	}

	// Get steam path.
	HKEY key;
	bool reg_success = false;
	if (RegOpenKeyEx( HKEY_LOCAL_MACHINE, "SOFTWARE\\Valve\\Steam\\", 0, KEY_QUERY_VALUE, &key ) == ERROR_SUCCESS) {
		DWORD reg_type;
		DWORD reg_size = DEFAULT_PATH_SIZE;

		// Resize if necessary.
		LSTATUS status = RegQueryValueEx( key, "InstallPath", nullptr, &reg_type, (LPBYTE)install_path.get_array(), &reg_size );
		if (status == ERROR_MORE_DATA) {
            if (!install_path.set_size( reg_size )) {
                stack->log( "Failed to allocate buffer for Steam install path." );
                return false;
            }

			// Now read into buffer.
			status = RegQueryValueEx( key, "InstallPath", nullptr, &reg_type, (LPBYTE)install_path.get_array(), &reg_size );
		}

		// Set directory to load Steam client DLL from.
		if (status == ERROR_SUCCESS) {
			SetDllDirectory( install_path.get_array() );
			reg_success = true;
		}
	}

	// Free buffer.
	install_path.clear();
	RegCloseKey( key );
	if (!reg_success) {
        stack->log( "Failed to get Steam install path from registry." );
        return false;
	}

	// Load the library.
	client_dll_ = LoadLibrary( "steamclient.dll" );
	if (client_dll_ == nullptr) {
        stack->log( "Failed to load steamclient.dll from Steam install path." );
        return false;
	}

	// Now define the functions.
	Steam_BGetCallback = (bool (*)(HSteamPipe, CallbackMsg_t *, HSteamCall *))GetProcAddress( client_dll_, "Steam_BGetCallback" );
	if (Steam_BGetCallback == nullptr) {
        stack->log( "Failed to get Steam_BGetCallback from Steam library." );
        return false;
	}
	Steam_FreeLastCallback = (void (*)(HSteamPipe))GetProcAddress( client_dll_, "Steam_FreeLastCallback" );
	if (Steam_FreeLastCallback == nullptr) {
        stack->log( "Failed to get Steam_FreeLastCallback from Steam library." );
        return false;
	}

	//Now load the API.
	CSteamAPILoader apiLoader;
	CreateInterfaceFn clientFactory = apiLoader.Load();
	if (clientFactory == nullptr) {
		stack->log( "Failed to create Steam client factory from API." );
        return false;
	}

	steam_client_ = (ISteamClient008*)clientFactory( STEAMCLIENT_INTERFACE_VERSION_008, 0 );
	if (steam_client_ == nullptr) {
        stack->log( "Failed to create Steam client interface." );
        return false;
	}

	// Down to to the nitty-gritty. Start 'er up!
	if (!SteamAPI_Init()) {
		stack->log( "Failed to initialize Steam API. Make sure Steam is running and try again." );
        return false;
	}

	pipe_ = steam_client_->CreateSteamPipe();
	user_ = steam_client_->ConnectToGlobalUser( pipe_ );

	// Make sure we've got a user.
	if (user_ == 0) {
        stack->log( "Failed to connect to global user." );
        return false;
	}

	// Make sure we're logged on correctly.
	steam_user_ = (ISteamUser012*)steam_client_->GetISteamUser(
		user_,
		pipe_,
		STEAMUSER_INTERFACE_VERSION_012 );
	if (steam_user_ == nullptr || !steam_user_->LoggedOn()) {
		stack->log( "You are not properly logged into Steam." );
        return false;
	}

	// Optional: older client interfaces expose Steam Cloud UGC for item decals.
	remote_storage_ = (ISteamRemoteStorage004*)steam_client_->GetISteamRemoteStorage(user_,pipe_,STEAMREMOTESTORAGE_INTERFACE_VERSION_004);
	steam_utils_ = (ISteamUtils005*)steam_client_->GetISteamUtils(pipe_,STEAMUTILS_INTERFACE_VERSION_005);
	steam_friends_ = (ISteamFriends009*)steam_client_->GetISteamFriends(user_,pipe_,STEAMFRIENDS_INTERFACE_VERSION_009);

    // Create game coordinator interface.
	game_coordinator_ = (ISteamGameCoordinator001*)steam_client_->GetISteamGenericInterface(
		user_, pipe_,
		STEAMGAMECOORDINATOR_INTERFACE_VERSION_001 );
	if (game_coordinator_ == nullptr) {
		stack->log( "Failed to get ISteamGameCoordinator interface." );
        return false;
	}
    return true;
#endif
}

/*
 * Clean up Steam interfaces.
 */
void Steam::close_interfaces( void )
{
#if !defined(_WIN32)
	if (api_initialized_ && api_shutdown_) api_shutdown_();
	api_initialized_=false;api_shutdown_=nullptr;api_dll_=nullptr;client_dll_=nullptr;
	user_=0;pipe_=0;steam_client_=nullptr;steam_user_=nullptr;game_coordinator_=nullptr;
	remote_storage_=nullptr;steam_utils_=nullptr;steam_friends_=nullptr;
	coordinator_welcomed_=false;
#else
    // Release user and pipe.
	if (steam_client_ != nullptr) {
        // Release user.
		if (user_ != 0) {
			steam_client_->ReleaseUser( pipe_, user_ );
			user_ = 0;
		}

        // Release pipe.
		if (pipe_ != 0) {
			steam_client_->ReleaseSteamPipe( pipe_ );
			pipe_ = 0;
		}
	}

    // Release DLL handle.
	if (client_dll_ != nullptr) {
		FreeLibrary( client_dll_ );
		client_dll_ = nullptr;
	}

	// Finally, shut down API.
	SteamAPI_Shutdown();
#endif
}

/*
 * Get callback waiting on Steam.
 */
bool Steam::get_callback( CallbackMsg_t* tCallback )
{
#if !defined(_WIN32)
	send_linux_hello_if_due();
	synthetic_callback_=false;
	HSteamCall steamCall;
	while (Steam_BGetCallback(pipe_,tCallback,&steamCall)) {
		uint32 size=0;
		if (tCallback->m_iCallback!=GCMessageAvailable_t::k_iCallback || has_message(&size)) return true;
		Steam_FreeLastCallback(pipe_);
	}
	uint32 size=0;
	if (game_coordinator_ && has_message(&size)) {
		*tCallback=CallbackMsg_t{};
		tCallback->m_iCallback=GCMessageAvailable_t::k_iCallback;
		synthetic_callback_=true;
		return true;
	}
	return false;
#else
	HSteamCall steamCall;
	return Steam_BGetCallback( pipe_, tCallback, &steamCall );
#endif
}

/*
 * Release previous callback.
 */
void Steam::release_callback( void ) const
{
#if !defined(_WIN32)
	if (synthetic_callback_) { synthetic_callback_=false; return; }
#endif
	Steam_FreeLastCallback( pipe_ );
}

/*
 * Check if message exists, and if it does, get message size.
 */
bool Steam::has_message( uint32* size ) const
{
	return game_coordinator_->IsMessageAvailable( size );
}

/*
 * Get message from coordinator.
 */
bool Steam::get_message( unsigned int* id, void* buffer, uint32 size, unsigned int* real_size )
{
    // Get error stack for reporting.
    JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();
	EGCResults result = game_coordinator_->RetrieveMessage( id, buffer, size, real_size );
    if (result != k_EGCResultOK) {
        stack->log( "Failed to retrieve message from game coordinator." );
        return false;
    }
#if !defined(_WIN32)
	const unsigned int bare_id=*id & ~PROTOBUF_MESSAGE_FLAG;
	if (bare_id==4004 || bare_id==k_ESOMsg_CacheSubscribed) coordinator_welcomed_=true;
#endif

	// Log the message before processing.
#if defined(LOG_STEAM_MESSAGES)
	if (log_ != nullptr) {
		fprintf( log_, "Received message of type 0x%x with size %u.\n", *id, *real_size );
		log_bytes( buffer, size );
	}
#endif
    return true;
}

/*
 * Send a message through coordinator.
 */
bool Steam::send_message( uint32 id, const void* buffer, uint32 size ) const
{
 #if defined(LOG_STEAM_MESSAGES)
	if (log_ != nullptr) {
		fprintf( log_, "Sending message of type 0x%x with size %d.\n", id, size );
		log_bytes( buffer, size );
	}
#endif

	EGCResults result = game_coordinator_->SendMessage( id, buffer, size );
    if (result != k_EGCResultOK) {
        return false;
    }
    return true;
}

/* Send a protobuf message. */
bool Steam::send_protobuf_message( const GCProtobufMessage* message ) const
{
	// Just send the message.
	if (!send_message( message->get_message_id(), 
		message->get_message_buffer(),
		message->get_message_size() ))
	{
		return false;
	}
	return true;
}

std::string Steam::get_gifter_name(uint32 account)
{
    if (!steam_friends_ || !account) return std::string();
    const CSteamID id(account,1,k_EUniversePublic,k_EAccountTypeIndividual);
    if (requested_gifters_.insert(account).second) steam_friends_->RequestUserInformation(id,true);
    const char* name=steam_friends_->GetFriendPersonaName(id);
    if (!name || !*name || std::string(name)=="[unknown]") return std::string();
    return name;
}

// Downloads only the Steam Cloud handle attached to the player's item. A
// missing cloud interface or unavailable decal must never prevent startup.
#if defined(_WIN32)
void Steam::request_decal(uint64 handle)
{
    if (!handle || !remote_storage_ || !steam_utils_ || decal_requested_.count(handle)) return;
    decal_requested_.insert(handle);
    CreateDirectoryA("thumbnail_renderer\\decals",nullptr);
    char png[128];sprintf_s(png,"thumbnail_renderer/decals/%016llx.png",handle);
    if (GetFileAttributesA(png)!=INVALID_FILE_ATTRIBUTES) return;
    SteamAPICall_t call=remote_storage_->UGCDownload(handle);
    if (call) decal_pending_[handle]=call;
}

void Steam::poll_decals()
{
    if (!remote_storage_ || !steam_utils_) return;
    for (auto it=decal_pending_.begin();it!=decal_pending_.end();) {
        bool failed=false;
        if (!steam_utils_->IsAPICallCompleted(it->second,&failed)) { ++it;continue; }
        const uint64 handle=it->first;
        AppId_t app=0;char* name=nullptr;int32 size=0;CSteamID owner;
        if (!failed && remote_storage_->GetUGCDetails(handle,&app,&name,&size,&owner) && app==440 && size>0 && size<=8*1024*1024) {
            std::vector<char> bytes(size);
            if (remote_storage_->UGCRead(handle,bytes.data(),size)==size) {
                char raw[128];sprintf_s(raw,"thumbnail_renderer/decals/%016llx.raw",handle);
                FILE* file=nullptr;
                if (fopen_s(&file,raw,"wb")==0 && file) {
                    const bool ok=fwrite(bytes.data(),1,bytes.size(),file)==bytes.size();fclose(file);
                    if (ok) {
                        char cmd[180];sprintf_s(cmd,"thumbnail_renderer\\decal_converter.exe %016llx",handle);
                        STARTUPINFOA si={};si.cb=sizeof(si);si.dwFlags=STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;
                        PROCESS_INFORMATION pi={};
                        if (CreateProcessA(nullptr,cmd,nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)) {
                            CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
                        }
                    }
                }
            }
        }
        it=decal_pending_.erase(it);
    }
}
#endif

/*
 * Get user Steam ID as 64-bit integer.
 */
uint64 Steam::get_steam_id( void ) const
{
	CSteamID steam_id = steam_user_->GetSteamID();
	return steam_id.ConvertToUint64();
}

#if defined( LOG_STEAM_MESSAGES )
/* Debug log function. */
void Steam::log_bytes( const void *buffer, unsigned int size ) const
{
	// Dump the bytes to file.
	const unsigned char *bytes = static_cast<const unsigned char*>(buffer);
	for (unsigned int i = 0; i < size; ++i) {
		fprintf( log_, "%02x ", bytes[i] );
	}
	fprintf( log_, "\n" );
	fflush( log_ );
}
#endif
