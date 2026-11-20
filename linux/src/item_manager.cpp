#include <stdlib.h>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#if defined(_WIN32)
#include <crtdbg.h>
#include <windows.h>
#include <process.h>
#include <shellapi.h>
#else
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <fstream>
#include <functional>
#include <cstring>
#include <math.h>
#include <json/json.h>
#include <jutil_version.hpp>
#include <jui/gfx/font_factory.hpp>
#include <jui/application/error_stack.hpp>
#include <jui/jui_version.hpp>
#include "serialized_buffer.hpp"
#include "slot_view.hpp"
#include "http_resource_loader.hpp"
#if defined(_WIN32)
#include "resource.h"
#else
constexpr int IDI_ICON1 = 0;
#endif
#include "item_manager.hpp"
#include "item_decorator.hpp"
#if defined(_WIN32)
#include "native/thumbnail_batch.hpp"
#endif

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#endif

// Temporarily disable equip and sort.
#define EQUIP_NOT_IMPLEMENTED
#define SORT_NOT_IMPLEMENTED

// Application attributes.
const JUTIL::ConstantString APPLICATION_TITLE = "Jengerer's Item Manager";
const JUTIL::ConstantString APPLICATION_VERSION = "Version 2.0.1";
const int APPLICATION_WIDTH	= 960;
const int APPLICATION_HEIGHT = 540;

// UI attributes.
const unsigned int EXIT_BUTTON_PADDING	= 10;
const DWORD PAGE_DELAY_INTERVAL = 500;
const unsigned int DEFAULT_PAGE_COUNT = 6;
const unsigned int TRIAL_PAGE_COUNT = 1;
const unsigned int TRIAL_SLOT_COUNT = 50;
const unsigned int PREMIUM_SLOT_COUNT = 300;

// Item manager resource loading URL.

// Error compilation strings.
const char* ERROR_FORMAT_FIRST = "#%u: %s";
const char* ERROR_FORMAT_NEXT = "\n\n#%u: %s";

/*
 * Item manager constructor.
 */
ItemManager::ItemManager( HINSTANCE instance )
	: Application( instance ),
	site_loader_( nullptr ),
	definition_loader_(nullptr),
	updater_( nullptr ),
	update_thread_( nullptr ),
#if defined(_WIN32)
	thumbnail_thread_( nullptr ),
#else
	thumbnail_thread_( 0 ),
#endif
	thumbnail_cancelled_( false ),
	thumbnail_refresh_pending_( false ),
	inventory_refreshing_( false ),
	is_pending_update_( false ),
	schema_(),
	inventory_(),
	steam_items_(),
	view_( nullptr ),
	title_(),
	think_function_( nullptr )
{
	// Set application size.
	set_size( APPLICATION_WIDTH, APPLICATION_HEIGHT );

	// Set window parameters.
	JUI::Window* window = get_window();
	window->set_border( true );
	window->set_fullscreen( false );
	window->set_icon( IDI_ICON1 );
	window->set_initial_client_size(1280,720);

	// Set inventory event handler.
    inventory_.set_listener( this );
}

/*
 * Item manager destructor.
 */
ItemManager::~ItemManager( void )
{
	close_interfaces();
}

/*
 * Loads basic resources for item manager.
 */
JUI::Application::ReturnStatus ItemManager::initialize( void )
{
    // Prepare stack for logging.
    JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

	// Get downloader.
	JUI::FileDownloader* downloader = JUI::FileDownloader::get_instance();
	if (downloader == nullptr) {
		stack->log( "Failed to create downloader object." );
		return PrecacheResourcesFailure;
	}
	// Allocate site loader.
	if (!JUTIL::BaseAllocator::allocate( &site_loader_ )) {
		stack->log( "Failed to allocate site loader object." );
		return PrecacheResourcesFailure;
	}
	site_loader_ = new (site_loader_) HttpResourceLoader( nullptr, downloader );

#if defined(_WIN32)
	// Create updater and remove previous files.
	if (!JUTIL::BaseAllocator::allocate( &updater_ )) {
		stack->log( "Failed to allocate object for updater." );
		return PrecacheResourcesFailure;
	}
	new (updater_) Updater( downloader, site_loader_, &APPLICATION_VERSION );

	// Remove previous files.
	if (!updater_->remove_old_files()) {
		return PrecacheResourcesFailure;
	}
#endif

	// Start up graphics and window.
    if (!title_.write( "%s (%s)", APPLICATION_TITLE.get_string(), APPLICATION_VERSION.get_string() )) {
        stack->log( "Failed to allocation application title." );
        return PrecacheResourcesFailure;
    }
    window_.set_title( &title_ );
	JUI::Application::ReturnStatus status = Application::initialize();
    if (status != Success) {
        return status;
    }

    // Create UI view and load necessary resources.
    if (!JUTIL::BaseAllocator::allocate( &view_ )) {
        stack->log( "Failed to allocate item manager view object." );
        return PrecacheResourcesFailure;
    }
    new (view_) ItemManagerView( &inventory_, &schema_ );
	view_->set_size( APPLICATION_WIDTH, APPLICATION_HEIGHT );
    view_->set_listener( this );
    if (!add( view_ )) {
        stack->log( "Failed to add item manager view object to application." );
        return PrecacheResourcesFailure;
    }
    if (!view_->download_resources( site_loader_ )) {
        return PrecacheResourcesFailure;
    }

    // Finish loading button and notice fonts so we can initialize view.
	if (!Notice::precache( &graphics_ )) {
        return PrecacheResourcesFailure;
    }
	else if (!Button::precache( &graphics_ )) {
        return PrecacheResourcesFailure;
    }

    // Initialize the view.
    if (!view_->initialize()) {
        return PrecacheResourcesFailure;
    }

#if defined(_WIN32)
	// Check if update required.
	UpdateCheckResult result = updater_->check_version_numbers();
	if (result == CHECK_FAILED) {
		return PrecacheResourcesFailure;
	}
	else if (result == UPDATE_REQUIRED &&
		MessageBoxA(nullptr,"A new Item Manager release is available on GitHub. Install it now?","Item Manager update",MB_YESNO|MB_ICONQUESTION)==IDYES) {
		const JUTIL::ConstantString notice("Downloading the Item Manager update...");
		if (!view_->set_loading_notice(&notice)) return PrecacheResourcesFailure;
		if (!JUTIL::BaseAllocator::allocate(&update_thread_)) return PrecacheResourcesFailure;
		new (update_thread_) std::thread(std::bind(&Updater::update,updater_));
		set_think(&ItemManager::updating);
	}
	else
#endif
	{
		// No update needed, load as normal.
#if defined(_WIN32)
		JUTIL::BaseAllocator::destroy( updater_ );
		updater_ = nullptr;
#endif

		// Base resources successful; load extra resources.
		if (!create_resources()) {
			const JUTIL::String* top = stack->get_top_error();
			if (!view_->create_error( top )) {
				return PrecacheResourcesFailure;
			}
			set_think( &ItemManager::exiting );
		}
		else {
			// Generate non-base layout.
			if (!view_->create_layout( &graphics_ )) {
				const JUTIL::String* top = stack->get_top_error();
				if (!view_->create_error( top )) {
					return PrecacheResourcesFailure;
				}
				set_think( &ItemManager::exiting );
			}

			// Start waiting for items to come in.
			const JUTIL::ConstantString PREPARING_MESSAGE = "Waiting for inventory message from Steam...";
			if (!view_->set_loading_notice( &PREPARING_MESSAGE )) {
				stack->log( "Failed to create loading notice." );
				return PrecacheResourcesFailure;
			}
			set_think( &ItemManager::waiting_for_items );
		}
	}

    // All base resources loaded successfully.
	render_ready_ = true;
	frame_limiter_.reset();
    return Success;
}

/*
 * Submit pending updated items and wait for response.
 */
void ItemManager::exit_application( void )
{
	// Either no pending item updates or user got impatient.
	Application::exit_application();
	set_think( &ItemManager::exiting );
}

bool ItemManager::create_resources( void ){
    // Stack for logging.
    JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

    // TASK: verify no other steam API application is running.

	// Download resources for inventory display.
	if (!ItemDecorator::download_resources( site_loader_ )) {
		return false;
	}

    // Enough resources loaded to show styled messages.
	if (!ItemDecorator::precache( &graphics_ )) {
		return false;
	}
	if (!ItemDisplay::precache()) {
        return false;
    }
	else if (!Notification::precache()) {
        return false;
    }

	// Initialize steam.
	if (!steam_items_.load_interfaces()) {
		return false;
	}
	steam_items_.set_listener( this );

    return true;
}

void ItemManager::close_interfaces( void )
{
	render_ready_ = false;
	cancel_thumbnail_preload();
    JUTIL::BaseAllocator::safe_destroy( &definition_loader_ );
    JUTIL::BaseAllocator::safe_destroy( &site_loader_ );
#if defined(_WIN32)
	JUTIL::BaseAllocator::safe_destroy( &updater_ );
#endif

	// Free cached resources.
	ItemDecorator::release();

	// Close font library.
	JUI::FontFactory::shut_down();

	// Free all protobuf library resources.
	google::protobuf::ShutdownProtobufLibrary();

	// Close downloader.
	JUI::FileDownloader::shut_down();
}

/*
 * Start loading definitions for schema.
 */
bool ItemManager::start_definition_load( void )
{
	// Set the message and redraw.
    const JUTIL::ConstantString LOADING_DEFINITION_MESSAGE = "Loading item definitions...";
    view_->set_loading_notice( &LOADING_DEFINITION_MESSAGE );

	// Set up loader.
    if (!JUTIL::BaseAllocator::allocate( &definition_loader_ )) {
        return false;
    }
    NotificationQueue* notifications = view_->get_notification_queue();
    definition_loader_ = new (definition_loader_) DefinitionLoader( &graphics_, &schema_, notifications );
    if (!definition_loader_->begin()) {
        return false;
    }
    return true;
}

/*
 * Resolve item and attribute definitions from loaded schema
 * and place them into the inventory.
 */
bool ItemManager::on_schema_loaded( void )
{
	// Resolve each inventory item's definition.
	unsigned int i;
	unsigned int end = inventory_.get_item_count();
	for (i = 0; i < end; ++i) {
		Item* item = inventory_.get_item( i );
		if (!schema_.resolve( item, &graphics_ )) {
			return false;
		}

		// Place item into inventory.
		if (!inventory_.place_item( item )) {
			return false;
		}
	}

	// Update excluded page state.
	view_->update_excluded_page_display();
	return true;
}

JUI::Application::ReturnStatus ItemManager::run( void )
{
    // Base application running and IO handling.
    JUI::Application::ReturnStatus result = Application::run();
    if (result != Success) {
        return result;
    }
	
    // Attempt to think; print error if failed.
    if (!think()) {
		// Get error stack.
		JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

        // Compile errors into one message.
		size_t length = stack->get_error_count();
		size_t i;
		const char* format_string = ERROR_FORMAT_FIRST;
		JUTIL::DynamicString error_string;
		for (i = 0; i < length; ++i) {
			const JUTIL::String* error = stack->get_error( i );
			if (!error_string.write( format_string, i + 1, error->get_string() )) {
				return NoMemoryFailure;
			}

			// Set next line to put a newline before.
			format_string = ERROR_FORMAT_NEXT;
		}
        if (!view_->create_error( &error_string )) {
            return NoMemoryFailure;
        }
        set_think( &ItemManager::exiting );
    }
	draw_frame();
	swap_buffers();

	frame_limiter_.wait();
    return Success;
}

void ItemManager::set_think( bool (ItemManager::*think_function)( void ) )
{
	think_function_ = think_function;
}

bool ItemManager::think( void )
{
	return (this->*think_function_)();
}

/*
 * Handle callbacks until inventory loaded event comes.
 */
bool ItemManager::waiting_for_items( void )
{
	// Get error stack.
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

	// Just wait for inventory message to change think.
	if (!steam_items_.handle_callbacks()) {
		stack->log( "Failed to handle callbacks while waiting for items." );
		return false;
	}
	return true;
}

/*
 * Handle definition load state.
 */
bool ItemManager::loading_schema( void )
{
	// Stack for reporting and getting.
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

    // Parse definition loading state.
	switch (definition_loader_->get_state()) {
		case LOADING_STATE_ERROR:
		{
			// Remove threaded loader.
			JUTIL::BaseAllocator::safe_destroy( &definition_loader_ );
			return false;
		}

		case LOADING_STATE_FINISHED:
		{
			// Remove threaded loader.
			JUTIL::BaseAllocator::safe_destroy( &definition_loader_ );

			// Resolve item metadata, then prepare local thumbnails while showing a notice.
			if (!on_schema_loaded()) {
				stack->log( "Failed to run post-schema load event." );
				return false;
			}
			if (start_thumbnail_preload()) set_think(&ItemManager::loading_thumbnails);
			else { apply_thumbnail_cache(); view_->destroy_loading_notice(); set_think(&ItemManager::running); }
			break;
		}

		default:
		{
            // Print message.
			const JUTIL::String* progress_message = definition_loader_->get_progress_message();
			if (progress_message == nullptr) {
				stack->log( "Failed to get progress message from loader." );
				return false;
			}
            if (!view_->set_loading_notice( progress_message )) {
				stack->log( "Failed to set progress message for loader." );
				return false;
			}
			break;
		}
	}

    // No problems.
    return true;
}

#if defined(_WIN32)
static unsigned __stdcall integrated_thumbnail_entry(void* context)
{
	try { return static_cast<unsigned>(run_thumbnail_batch(*static_cast<std::atomic<bool>*>(context))); }
	catch (...) { return 1; }
}
#endif

void ItemManager::cancel_thumbnail_preload(void)
{
	if (!thumbnail_thread_) return;
	thumbnail_cancelled_.store(true);
#if defined(_WIN32)
	WaitForSingleObject(thumbnail_thread_,INFINITE);
	CloseHandle(thumbnail_thread_);
	thumbnail_thread_=nullptr;
#else
	kill(thumbnail_thread_,SIGTERM);
	while (waitpid(thumbnail_thread_,nullptr,0)<0 && errno==EINTR) {}
	thumbnail_thread_=0;
#endif
}

bool ItemManager::start_thumbnail_preload(void)
{
	if (thumbnail_thread_) { thumbnail_refresh_pending_=true; return true; }
	std::error_code cache_error;
	std::filesystem::create_directories("thumbnail_renderer/cache",cache_error);
	if (cache_error) return false;
	Json::Value root(Json::objectValue), entries(Json::arrayValue);
	bool missing=false;
	for (unsigned int i=0;i<inventory_.get_item_count();++i) {
		Item* item=inventory_.get_item(i);
		const ItemPresentation& p=item->get_presentation();
		if (!p.decorated || !p.paint_id || !p.has_wear) continue;
		uint32 wearbits=0;memcpy(&wearbits,&p.wear_value,sizeof(wearbits));
		const uint64 seed=item->get_paint_seed();
		char key[120];std::snprintf(key,sizeof(key),"05-%04x-%08x-%016llx-%08x-%d-%08x",item->get_type_index(),p.paint_id,static_cast<unsigned long long>(seed),wearbits,p.stat_clock ? 1 : 0,p.stat_clock ? p.stat_clock_count : 0);
		std::string prefix=std::string("thumbnail_renderer/cache/")+key;
		item->set_thumbnail_path(prefix+"-display.png");
		if (!std::filesystem::is_regular_file(item->get_thumbnail_path())) missing=true;
		Json::Value entry(Json::objectValue);
		entry["key"]=key;entry["definition"]=item->get_type_index();entry["paint"]=p.paint_id;
		entry["seed"]=std::to_string(seed);entry["wear"]=p.wear_value;entry["stat_clock"]=p.stat_clock;entry["stat_clock_count"]=p.stat_clock_count;entry["stat_clock_scale"]=p.stat_clock_scale;entries.append(entry);
	}
	if (entries.empty() || !missing) return false;
	root["items"]=entries;
	std::ofstream manifest("thumbnail_renderer/manifest.json",std::ios::binary|std::ios::trunc);
	if (!manifest) return false;
	Json::StreamWriterBuilder builder;builder["indentation"]="";
	manifest << Json::writeString(builder,root);manifest.close();
	if (!manifest) return false;
	{std::ofstream progress("thumbnail_renderer/progress.txt",std::ios::trunc);progress << "Preparing War Paint thumbnails...";}
	const JUTIL::ConstantString notice("Preparing War Paint thumbnails...\nThis may take a while the first time.");
	view_->set_loading_notice(&notice);
	thumbnail_cancelled_.store(false);
#if defined(_WIN32)
	thumbnail_thread_=reinterpret_cast<HANDLE>(_beginthreadex(nullptr,0,integrated_thumbnail_entry,&thumbnail_cancelled_,0,nullptr));
#else
	char program[]="python3",script[]="thumbnail_renderer/batch.py";
	char* arguments[]={program,script,nullptr};pid_t pid=0;
	thumbnail_thread_=posix_spawnp(&pid,program,nullptr,nullptr,arguments,environ)==0 ? static_cast<int>(pid) : 0;
#endif
	if (!thumbnail_thread_) {
		const JUTIL::ConstantString error("Could not start the thumbnail renderer; existing item images will be used.");
		view_->create_alert(&error);return false;
	}
	return true;
}

void ItemManager::apply_thumbnail_cache(void)
{
	for (unsigned int i=0;i<inventory_.get_item_count();++i) {
		Item* item=inventory_.get_item(i);
		steam_items_.request_decal(item->get_presentation().decal_handle);
		const std::string& path=item->get_thumbnail_path();
		if (path.empty() || !std::filesystem::is_regular_file(path)) continue;
		JUTIL::ConstantString filename(path.c_str());JUI::FileTexture* texture=nullptr;
		if (graphics_.get_texture(&filename,&texture)==JUI::Graphics2D::Success) item->set_thumbnail_texture(texture);
	}
}

void ItemManager::update_decals( void )
{
    steam_items_.poll_decals();
    static DWORD last_check=0;
    const DWORD now=GetTickCount();
    if (now-last_check<500) return;
    last_check=now;
    for (unsigned int i=0;i<inventory_.get_item_count();++i) {
        Item* item=inventory_.get_item(i);
        const unsigned int crafter_account=item->get_presentation().crafter_account;
        if (crafter_account) {
            const std::string name=steam_items_.get_gifter_name(crafter_account);
            if (!name.empty() && name!=item->get_presentation().crafter_name) {
                item->set_crafter_name(name);view_->refresh_gift_display(item);
            }
        }
        const unsigned int bond_1=item->get_presentation().bond_account_1;
        if (bond_1) {
            const std::string name=steam_items_.get_gifter_name(bond_1);
            if (!name.empty() && name!=item->get_presentation().bond_name_1) {
                item->set_bond_name_1(name);view_->refresh_gift_display(item);
            }
        }
        const unsigned int bond_2=item->get_presentation().bond_account_2;
        if (bond_2) {
            const std::string name=steam_items_.get_gifter_name(bond_2);
            if (!name.empty() && name!=item->get_presentation().bond_name_2) {
                item->set_bond_name_2(name);view_->refresh_gift_display(item);
            }
        }
        const unsigned int gift_account=item->get_presentation().gift_account;
        if (gift_account) {
            const std::string name=steam_items_.get_gifter_name(gift_account);
            if (!name.empty() && name!=item->get_presentation().gift_name) {
                item->set_gift_name(name);view_->refresh_gift_display(item);
            }
        }
        const uint64 handle=item->get_presentation().decal_handle;
        if (!handle || item->get_decal_texture()) continue;
        char path[128];std::snprintf(path,sizeof(path),"thumbnail_renderer/decals/%016llx.png",static_cast<unsigned long long>(handle));
        if (!std::filesystem::is_regular_file(path)) continue;
        JUTIL::ConstantString filename(path);JUI::FileTexture* texture=nullptr;
        if (graphics_.get_texture(&filename,&texture)==JUI::Graphics2D::Success) item->set_decal_texture(texture);
    }
}

bool ItemManager::loading_thumbnails(void)
{
	if (!steam_items_.handle_callbacks()) return false;
	update_decals();
	if (think_function_ != &ItemManager::loading_thumbnails) return true;
	if (!thumbnail_thread_) { view_->destroy_loading_notice();set_think(&ItemManager::running);return true; }
#if defined(_WIN32)
	const bool worker_running=WaitForSingleObject(thumbnail_thread_,0)==WAIT_TIMEOUT;
#else
	int process_status=0;
	const pid_t wait_result=waitpid(thumbnail_thread_,&process_status,WNOHANG);
	const bool worker_running=wait_result==0 || (wait_result<0 && errno==EINTR);
#endif
	if (worker_running) {
		std::ifstream progress("thumbnail_renderer/progress.txt");std::string line;
		if (std::getline(progress,line) && !line.empty()) {
			JUTIL::DynamicString notice;
			if (notice.write("%s\nHigh-quality images are cached for future launches.",line.c_str())) view_->set_loading_notice(&notice);
		}
		return true;
	}
#if defined(_WIN32)
	DWORD worker_exit=0;GetExitCodeThread(thumbnail_thread_,&worker_exit);
	CloseHandle(thumbnail_thread_);thumbnail_thread_=nullptr;
#else
	const int worker_exit=wait_result>0 && WIFEXITED(process_status) ? WEXITSTATUS(process_status) : 1;
	thumbnail_thread_=0;
#endif
	apply_thumbnail_cache();
	if (thumbnail_refresh_pending_) {
		thumbnail_refresh_pending_=false;
		if (start_thumbnail_preload()) return true;
		apply_thumbnail_cache();
	}
	view_->destroy_loading_notice();
	std::ifstream progress("thumbnail_renderer/progress.txt");std::string result;std::getline(progress,result);
	if (worker_exit!=0 || result.find("unavailable")!=std::string::npos) {
		const JUTIL::ConstantString error("Some thumbnails could not be rendered. Existing item images will be used for those items. See thumbnail_renderer/thumbnail-errors.log for details.");
		view_->create_alert(&error);
	}
	set_think(&ItemManager::running);return true;
}

bool ItemManager::running( void )
{
	if (!steam_items_.handle_callbacks()){
		return false;
	}
	update_decals();
	if (!view_->on_enter_frame()) {
		return false;
	}
    return true;
}

/*
 * Waiting for user to trigger update.
 */
bool ItemManager::pending_update( void )
{
	return true;
}

/*
 * Waiting for update to complete.
 */
bool ItemManager::updating( void )
{
#if !defined(_WIN32)
	return false;
#else
	// Stack for errors.
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

	// Don't try join until it's finished.
	UpdateStatus status = updater_->get_update_status();
	if (status != UPDATE_PENDING) {
		update_thread_->join();
		JUTIL::BaseAllocator::destroy( update_thread_ );

		// Check return value.
		if (status == UPDATE_FAILED) {
			return false;
		}

		// Run new item manager.
		if (!updater_->run_new_version()) {
			stack->log( "Failed to run the new version of the item manager. I fail miserably, try to run it yourself." );
			return false;
		}
		exit_application();
	}
	return true;
#endif
}

/*
 * Waiting for application to close.
 */
bool ItemManager::exiting( void )
{
    return true;
}

/*
 * Handle error acknowledge event from view.
 */
bool ItemManager::on_error_acknowledged( void )
{
	// Stack for reporting errors.
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

#if defined(_WIN32)
	// Check if we should update.
	if (is_pending_update_) {
		// Unset pending for next error.
		is_pending_update_ = false;

		// Tell user we're updating now.
		const JUTIL::ConstantString UPDATING_NOTICE( "Updating the item manager..." );
		if (!view_->set_loading_notice( &UPDATING_NOTICE )) {
			stack->log( "Failed to create updating notice." );
			return false;
		}

		// Create thread for updating.
		if (!JUTIL::BaseAllocator::allocate( &update_thread_ )) {
			stack->log( "Failed to create updater thread." );
			return false;
		}
		new (update_thread_) std::thread( std::bind( &Updater::update, updater_ ) );
		set_think( &ItemManager::updating );
	}
	else {
		exit_application();
	}
#else
	exit_application();
#endif
    return true;
}

/*
 * Triggered when the view begins moving some items.
 */
void ItemManager::on_item_begin_move( void )
{
	steam_items_.clear_item_updates();
}

/*
 * Handle item update event and trigger Steam message.
 */
bool ItemManager::on_item_moved( Item* item, unsigned int index )
{
	// Send request to update position.
	inventory_.displace_item( item );
	inventory_.move_item( item, index );
	if (!steam_items_.add_item_update( item )) {
		return false;
	}
	return true;
}

/*
 * Triggered when the view is finished moving some items.
 */
bool ItemManager::on_item_end_move( void )
{
	if (!steam_items_.push_item_updates()) {
		return false;
	}
	return true;
}

/*
 * Handle item crafting.
 */
bool ItemManager::on_craft_items( void )
{
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

	const JUTIL::ConstantString CRAFT_ERROR( "Failed to send craft message." );
	DynamicSlotArray* selected = inventory_.get_selected_slots();
	unsigned int count = selected->get_size();
	unsigned int i;
	if (steam_items_.set_craft_size( count )) {
		for (i = 0; i < count; ++i) {
			Item* item = selected->get_item( i );
			steam_items_.set_craft_item( i, item );
		}
		if (steam_items_.craft_items()) {
			return true;
		}
	}

	// Create an alert here.
	if (!view_->create_alert( &CRAFT_ERROR )) {
		stack->log( "Failed to write craft error message." );
		return false;
	}
	return true;
}

/*
 * Handle item deletion.
 */
bool ItemManager::on_delete_item( void )
{
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

	const JUTIL::ConstantString DELETE_ERROR( "Failed to send delete message." );
	DynamicSlotArray* selected = inventory_.get_selected_slots();
	unsigned int count = selected->get_size();
	if (count != 1) {
		stack->log( "Attempting to delete more than one item." );
		return false;
	}

	// Delete immediately and send message.
	Item* item = selected->get_item( 0 );
	inventory_.delete_item( item );
	if (!steam_items_.delete_item( item )) {
		stack->log( "Failed to send item delete message." );
		return false;
	}

	return true;
}

/*
 * Handle selection set change.
 */
void ItemManager::on_selection_changed( void )
{
	view_->update_buttons_state();
}

/*
 * Handle item creation event.
 */
bool ItemManager::on_item_created( Item* item )
{
	// Add item to inventory first.
	if (!inventory_.add_item( item )) {
		return false;
	}

	if (schema_.is_loaded()) {
		// Resolve item definition.
		if (!schema_.resolve( item, &graphics_ )) {
			return false;
		}

		// Place into slot.
		if (!inventory_.place_item( item )) {
			return false;
		}

		// Update excluded slot in case we pushed up.
		view_->update_excluded_page_display();
		if (!inventory_refreshing_) {
			if (start_thumbnail_preload()) set_think(&ItemManager::loading_thumbnails);
			else apply_thumbnail_cache();
		}
	}

	return true;
}

/*
 * Handle item deletion event.
 */
void ItemManager::on_item_deleted( uint64 id )
{
	// Find item in inventory.
	Item* item = inventory_.find_item( id );
	if (item != nullptr) {
		inventory_.delete_item( item );

		// Update excluded in case we popped.
		view_->update_excluded_page_display();

		// Update button state since selection may have changed.
		view_->update_buttons_state();
	}
}

/*
 * Handle item update event.
 */
bool ItemManager::on_item_updated( uint64 id, uint32 flags )
{
	// Get error stack.
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

	// Find item in inventory.
	Item* item = inventory_.find_item( id );
	if (item == nullptr) {
		stack->log( "Failed to find item to update." );
		return false;
	}
	item->set_inventory_flags( flags );

	// Move to new spot; assume Steam will fix over-riding.
	uint32 index = item->get_position();
	inventory_.displace_item( item );
	if (!inventory_.move_item( item, index )) {
		stack->log( "Failed to place updated item." );
		return false;
	}

	// Update excluded in case we moved out.
	view_->update_excluded_page_display();
	return true;
}

/*
 * Handle craft failure event.
 */
void ItemManager::on_craft_failed( void )
{
	// Post alert.
	const JUTIL::ConstantString CRAFT_ERROR( "Ze item combination. It does nothing! Craft failed!" );
	view_->create_alert( &CRAFT_ERROR );
}

/*
 * Handle inventory size change event.
 */
bool ItemManager::on_inventory_resize( bool is_trial_account, uint32 extra_slots )
{
	// Get error stack.
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

    // Calculate number of slots to resize to.
    unsigned int total_slots;
    if (is_trial_account) {
        // Should be no extra slots for trial accounts.
        total_slots = TRIAL_SLOT_COUNT;
    }
    else {
        total_slots = PREMIUM_SLOT_COUNT + extra_slots;
    }
	SlotArray* inventory = inventory_.get_inventory_slots();
    if (total_slots < inventory->get_size()) {
		stack->log( "New backpack is smaller than the old one." );
        return false;
    }

    // Attempt to set inventory book size.
    if (!inventory->set_size( total_slots )) {
        return false;
    }
    return true;
}

/*
 * Handle inventory reset.
 */
void ItemManager::on_inventory_reset( void )
{
	inventory_refreshing_=true;thumbnail_refresh_pending_=false;
	cancel_thumbnail_preload();
	// TODO: tell view to drop selection from here.
	inventory_.delete_items();
}

/*
 * Handle inventory load completion event.
 */
bool ItemManager::on_inventory_loaded( void )
{
	inventory_refreshing_=false;
	// Create thread to load item definitions.
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();
	if (!start_definition_load()) {
		return false;
	}

	// Update page display.
	if (!view_->update_page_display()) {
		stack->log( "Failed to update page display." );
		return false;
	}
	set_think( &ItemManager::loading_schema );
	return true;
}

JUI::IOResult ItemManager::on_mouse_clicked( JUI::Mouse* mouse )
{
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

    // Pass message to item manager view.
    JUI::IOResult result = view_->on_mouse_clicked( mouse );
	if (result == JUI::IO_RESULT_ERROR) {
		const JUTIL::String* top = stack->get_top_error();
		if (!view_->create_error( top )) {
			MessageBox( nullptr, top->get_string(), "Error while handling mouse click event!", MB_ICONERROR | MB_OK );
		}
		set_think( &ItemManager::exiting );
	}
    return result;
}

JUI::IOResult ItemManager::on_mouse_released( JUI::Mouse* mouse )
{
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

    // Pass message to item manager view.
    JUI::IOResult result = view_->on_mouse_released( mouse );
	if (result == JUI::IO_RESULT_ERROR) {
		const JUTIL::String* top = stack->get_top_error();
		if (!view_->create_error( top )) {
			MessageBox( nullptr, top->get_string(), "Error while handling mouse release event!", MB_ICONERROR | MB_OK );
		}
		set_think( &ItemManager::exiting );
	}
    return result;
}

JUI::IOResult ItemManager::on_mouse_moved( JUI::Mouse* mouse )
{
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

	// Pass message to item manager view.
    JUI::IOResult result = view_->on_mouse_moved( mouse );
	if (result == JUI::IO_RESULT_ERROR) {
		const JUTIL::String* top = stack->get_top_error();
		if (!view_->create_error( top )) {
			MessageBox( nullptr, top->get_string(), "Error while handling mouse move event!", MB_ICONERROR | MB_OK );
		}
		set_think( &ItemManager::exiting );
	}
    return result;
}

/*
 * Handle keyboard pressed input.
 */
JUI::IOResult ItemManager::on_key_pressed( int key )
{
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

	// Pass message to item manager view.
	JUI::IOResult result = view_->on_key_pressed( key );
	if (result == JUI::IO_RESULT_ERROR) {
		const JUTIL::String* top = stack->get_top_error();
		if (!view_->create_error( top )) {
			MessageBox( nullptr, top->get_string(), "Error while handling key press event!", MB_ICONERROR | MB_OK );
		}
		set_think( &ItemManager::exiting );
	}
	return result;
}

/*
 * Handle keyboard release input.
 */
JUI::IOResult ItemManager::on_key_released( int key )
{
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

	// Pass message to item manager view.
	JUI::IOResult result = view_->on_key_released( key );
	if (result == JUI::IO_RESULT_ERROR) {
		const JUTIL::String* top = stack->get_top_error();
		if (!view_->create_error( top )) {
			MessageBox( nullptr, top->get_string(), "Error while handling key press event!", MB_ICONERROR | MB_OK );
		}
		set_think( &ItemManager::exiting );
	}
	return JUI::IO_RESULT_UNHANDLED;
}

void ItemManager::on_focus_lost()
{
	if (view_) view_->on_focus_lost();
}
