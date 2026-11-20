#include <jui/application/error_stack.hpp>
#include <jui/gfx/font_interface.hpp>
#include <jui/gfx/font_factory.hpp>
#include "item_display.hpp"
#include "spell_colours.hpp"
#include <ctime>

// Title style attributes.
const JUTIL::ConstantString ITEM_DISPLAY_TITLE_FONT_FACE = "fonts/tf2build.ttf";
const unsigned int ITEM_DISPLAY_TITLE_FONT_SIZE	= 13;

// Information style attributes.
const JUTIL::ConstantString* ITEM_DISPLAY_INFO_FONT_FACE = &ITEM_DISPLAY_TITLE_FONT_FACE;
const unsigned int ITEM_DISPLAY_INFO_FONT_SIZE = 10;

// Display colour.
const JUI::Colour& ITEM_DISPLAY_COLOUR = JUI::COLOUR_BLACK;

// Name fonts.
JUI::FontInterface *ItemDisplay::name_font_ = nullptr;
JUI::FontInterface *ItemDisplay::info_font_ = nullptr;

// Item dipslay layout attributes.
const unsigned int ITEM_DISPLAY_PADDING = 20;
const unsigned int ITEM_DISPLAY_SPACING = 5;
const unsigned int ITEM_DISPLAY_RADIUS = 5;
const int ITEM_DISPLAY_TEXT_WIDTH = 300;
const float ITEM_DISPLAY_ALPHA_SPEED = 0.1f;
const float ITEM_DISPLAY_ALPHA_MAX = 0.8f;

/*
 * Item display constructor.
 */
ItemDisplay::ItemDisplay( ItemSchema* schema ) : RoundedRectangleContainer( ITEM_DISPLAY_PADDING ), last_alpha_time_(GetTickCount64())
{
	// Set default attributes.
	set_alpha( 0 );
	set_item( nullptr );
	set_active( false );

	schema_ = schema;

	// Null objects.
	name_text_ = nullptr;
	text_layout_ = nullptr;
}

/*
 * Item display destructor.
 */
ItemDisplay::~ItemDisplay( void )
{
	// Nothing.
}

/*
 * Initialize UI elements for display.
 */
bool ItemDisplay::initialize( void )
{
	// Set up rectangle container.
	if (!RoundedRectangleContainer::initialize())
	{
		return false;
	}
	RoundedRectangle* rectangle = get_rounded_rectangle();
	rectangle->set_radius( ITEM_DISPLAY_RADIUS );
	rectangle->set_colour( &ITEM_DISPLAY_COLOUR );

	// Create layout.
	if (!JUTIL::BaseAllocator::allocate( &text_layout_ )) {
		return false;
	}
	text_layout_ = new (text_layout_) JUI::VerticalLayout();
	if (!add( text_layout_ )) {
		JUTIL::BaseAllocator::destroy( text_layout_ );
		return false;
	}
	set_content( text_layout_ );
	
	// Create name text element.
	if (!JUTIL::BaseAllocator::allocate( &name_text_ )) {
		return false;
	}
	name_text_ = new (name_text_) JUI::WrappedText( name_font_, ITEM_DISPLAY_TEXT_WIDTH );
	if (!text_layout_->add( name_text_ )) {
		JUTIL::BaseAllocator::destroy( name_text_ );
        return false;
	}
    name_text_->set_text_formatting( DT_CENTER );

	// Pack so we can create a temporary rectangle.
	pack();
    return true;
}

/*
 * Update item display.
 */
bool ItemDisplay::update_display( void )
{
	// Stack for error logging.
	JUI::ErrorStack* stack = JUI::ErrorStack::get_instance();

	// Alter display based on quality.
	name_text_->set_colour( item_->get_quality_colour() );

	JUTIL::DynamicString item_name;
	const ItemPresentation& presentation = item_->get_presentation();
	if (!item_->is_renamed() && (presentation.decorated || presentation.unusual)) {
		if (presentation.strange) item_name.write("Strange");
		if (presentation.unusual) item_name.write(presentation.strange ? " Unusual" : "Unusual");
	}
	else if(!presentation.force_unique && item_->get_quality() != k_EItemQuality_Common && !item_->is_renamed()){
		// Prepend strange level name or quality name.
		const JUTIL::String* prefix = nullptr;
		if (item_->get_quality() == k_EItemQuality_Strange){
			// Get first kill eater value for this item.
			// TODO: Maybe parse kill eaters on item creation.
			uint32 strange_value = 0;
			if (!item_->get_kill_eater_value( 0, &strange_value )) {
				stack->log( "Found strange item with no strange type at first index." );
				return false;
			}
			uint32 strange_index;
			if (!item_->get_kill_eater_type( 0, &strange_index )) {
				strange_index = DEFAULT_STRANGE_TYPE;
			}
			const KillEaterType* type = schema_->get_kill_eater_type( strange_index );
			if (type == nullptr) {
				stack->log( "Bad strange type index." );
				return false;
			}
			const KillEaterRanks* ranks = type->get_levels();
			const KillEaterLevel* level = ranks->find_level( strange_value );
			prefix = level->get_prefix();
		}
		else {
			prefix = schema_->get_quality_name( item_->get_quality() );
		}
		if (prefix != nullptr) {
			if (!item_name.copy( prefix )) {
				stack->log( "Failed to write prefix before item name." );
				return false;
			}
		}
	}

	// Write the actual item name after.
	if (!item_name.write( "%s%s", item_name.get_length() ? " " : "", item_->get_name()->get_string()) ) {
		return false;
	}

	// Get item craft number.
	uint32 craft_number;
	if (item_->get_craft_number( &craft_number )) {
		if (!item_name.write( " #%u", craft_number )) {
			return false;
		}
	}

	// Convert to multi-byte.
    JUTIL::DynamicWideString wide_string;
	int wide_size = MultiByteToWideChar( CP_UTF8, 0, item_name.get_string(), item_name.get_length(), nullptr, 0 );
    if (!wide_string.set_length( wide_size )) {
        return false;
    }

	// Convert to wide string.
	MultiByteToWideChar( CP_UTF8, 0, item_name.get_string(), item_name.get_length(), wide_string.get_string(), wide_size );
	set_name( &wide_string );

    // Rebuild only rows with actual content; gaps belong before sections, never after them.
    for (auto* row : detail_rows_) { text_layout_->remove(row); JUTIL::BaseAllocator::destroy(row); }
    detail_rows_.clear();detail_gaps_.clear();animated_spell_rows_.clear();
    auto set_utf8 = [&](JUI::Text* target,const std::string& text) {
        JUTIL::DynamicWideString wide;
        const int n=MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0);
        if (n<=0 || !wide.set_length(n)) return false;
        MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),wide.get_string(),n);
        target->set_text(&wide);return true;
    };
    auto append_row = [&](const std::string& text,const JUI::Colour& colour,int gap,int paint_index=-1) {
        if (text.empty()) return true;
        JUI::WrappedText* row=nullptr;
        if (!JUTIL::BaseAllocator::allocate(&row)) return false;
        row=new (row) JUI::WrappedText(info_font_,ITEM_DISPLAY_TEXT_WIDTH);
        if (!set_utf8(row,text)) { JUTIL::BaseAllocator::destroy(row);return false; }
        row->set_colour(&colour);
        if (!text_layout_->add(row)) { JUTIL::BaseAllocator::destroy(row);return false; }
        detail_rows_.push_back(row);detail_gaps_.push_back(gap);
        if (paint_index>=0) animated_spell_rows_.push_back(std::make_pair(row,static_cast<unsigned int>(paint_index)));
        return true;
    };
    auto append_coloured_value = [&](const std::string& label,const std::string& value,const JUI::Colour& colour,int gap) {
        if (value.empty()) return true;
        JUI::Text *left=nullptr,*right=nullptr;
        if (!JUTIL::BaseAllocator::allocate(&left)) return false;
        left=new (left) JUI::Text(info_font_);
        if (!JUTIL::BaseAllocator::allocate(&right)) { JUTIL::BaseAllocator::destroy(left);return false; }
        right=new (right) JUI::Text(info_font_);
        if (!set_utf8(left,label) || !set_utf8(right,value)) { JUTIL::BaseAllocator::destroy(left);JUTIL::BaseAllocator::destroy(right);return false; }
        left->set_colour(&JUI::COLOUR_WHITE);right->set_colour(&colour);
        JUI::Component* row=nullptr;
        if (left->get_width()+right->get_width()<=ITEM_DISPLAY_TEXT_WIDTH) {
            JUI::HorizontalLayout* horizontal=nullptr;
            if (!JUTIL::BaseAllocator::allocate(&horizontal)) { JUTIL::BaseAllocator::destroy(left);JUTIL::BaseAllocator::destroy(right);return false; }
            horizontal=new (horizontal) JUI::HorizontalLayout();
            if (!horizontal->add(left)) { JUTIL::BaseAllocator::destroy(left);JUTIL::BaseAllocator::destroy(right);JUTIL::BaseAllocator::destroy(horizontal);return false; }
            if (!horizontal->add(right)) { JUTIL::BaseAllocator::destroy(right);JUTIL::BaseAllocator::destroy(horizontal);return false; }
            horizontal->pack(0,JUI::ALIGN_MIDDLE);row=horizontal;
        } else {
            // Long paint names wrap to a second centered line, preserving their colour.
            JUTIL::BaseAllocator::destroy(right);
            JUI::WrappedText* wrapped=nullptr;
            if (!JUTIL::BaseAllocator::allocate(&wrapped)) { JUTIL::BaseAllocator::destroy(left);return false; }
            wrapped=new (wrapped) JUI::WrappedText(info_font_,ITEM_DISPLAY_TEXT_WIDTH);
            if (!set_utf8(wrapped,value)) { JUTIL::BaseAllocator::destroy(left);JUTIL::BaseAllocator::destroy(wrapped);return false; }
            wrapped->set_colour(&colour);
            JUI::VerticalLayout* vertical=nullptr;
            if (!JUTIL::BaseAllocator::allocate(&vertical)) { JUTIL::BaseAllocator::destroy(left);JUTIL::BaseAllocator::destroy(wrapped);return false; }
            vertical=new (vertical) JUI::VerticalLayout();
            if (!vertical->add(left)) { JUTIL::BaseAllocator::destroy(left);JUTIL::BaseAllocator::destroy(wrapped);JUTIL::BaseAllocator::destroy(vertical);return false; }
            if (!vertical->add(wrapped)) { JUTIL::BaseAllocator::destroy(wrapped);JUTIL::BaseAllocator::destroy(vertical);return false; }
            vertical->pack(4,JUI::ALIGN_CENTER);row=vertical;
        }
        if (!text_layout_->add(row)) { JUTIL::BaseAllocator::destroy(row);return false; }
        detail_rows_.push_back(row);detail_gaps_.push_back(gap);return true;
    };
    const int line_gap=4, section_gap=8, title_gap=6;
    const bool has_grade=presentation.grade!=0;
    if (has_grade && !append_row(ItemPresentation::grade_name(presentation.grade),JUI::Colour(ItemPresentation::grade_colour(presentation.grade)),title_gap)) return false;
    if (!append_row("Level "+std::to_string(item_->get_level()),JUI::COLOUR_WHITE,has_grade ? line_gap : title_gap)) return false;
    if (presentation.crafter_account) {
        const std::string crafter=presentation.crafter_name.empty() ? "Steam user #"+std::to_string(presentation.crafter_account) : presentation.crafter_name;
        if (!append_row("Crafted by: "+crafter,JUI::Colour(0x8CC8F2),section_gap)) return false;
    }
    if (presentation.festivized && !append_row("Festivized",JUI::Colour(0xFFD700),section_gap)) return false;
    bool prior_section=has_grade || presentation.crafter_account || presentation.festivized;
    bool spell_started=false;
    for (const auto& spell : presentation.spells) {
        const JUI::Colour colour(spell.paint_index>=0 ? spell_paint_colour(spell.paint_index,GetTickCount64()) : spell.colour);
        if (!append_row("Halloween Spell: "+spell.name,colour,spell_started ? line_gap : section_gap,spell.paint_index)) return false;
        spell_started=true;prior_section=true;
    }
    bool has_killstreak=presentation.killstreak_tier>0;
    if (has_killstreak) {
        int gap=section_gap;
        if (!presentation.killstreaker.empty()) {
            if (!append_row("Killstreaker: "+presentation.killstreaker,JUI::COLOUR_WHITE,gap)) return false;
            gap=line_gap;
        }
        if (!presentation.sheen.empty()) {
            if (!append_coloured_value("Sheen: ",presentation.sheen,JUI::Colour(presentation.sheen_colour),gap)) return false;
            gap=line_gap;
        }
        if (!append_row("Killstreaks Active",JUI::COLOUR_WHITE,gap)) return false;
        prior_section=true;
    }
    std::string finish;
    if (!presentation.finish.empty()) finish="Finish: "+presentation.finish;
    if (!presentation.wear.empty()) finish+=(finish.empty()?"":"\n")+std::string("Exterior: ")+presentation.wear;
    if (!finish.empty()) {
        if (!append_row(finish,JUI::COLOUR_WHITE,prior_section ? section_gap : line_gap)) return false;
        prior_section=true;
    }
    bool quality_started=false;
    if (presentation.strange) {
        if (!append_coloured_value("Quality: ","Strange",JUI::Colour(0xCF6A32),section_gap)) return false;
        quality_started=true;
        for (uint32 i=0;i<STRANGE_TYPE_COUNT;++i) {
            uint32 count=0,eater_type=DEFAULT_STRANGE_TYPE;
            if (!item_->get_kill_eater_value(i,&count)) continue;
            item_->get_kill_eater_type(i,&eater_type);
            const KillEaterType* type=schema_->get_kill_eater_type(eater_type);
            if (type && !append_row(std::string(type->get_description()->get_string())+": "+std::to_string(count),JUI::COLOUR_WHITE,line_gap)) return false;
        }
    }
    if (presentation.unusual) {
        if (!append_coloured_value("Quality: ","Unusual",JUI::Colour(0x8650AC),section_gap)) return false;
        quality_started=true;
    }
    if (!presentation.effects.empty()) {
        if (!append_row(presentation.effects,JUI::COLOUR_WHITE,quality_started ? line_gap : section_gap)) return false;
        quality_started=true;
    }
    if (quality_started) prior_section=true;
    if (!presentation.paint_colour_name.empty()) {
        if (!append_coloured_value("Paint Color: ",presentation.paint_colour_name,JUI::Colour(presentation.paint_colour_rgb),prior_section ? section_gap : line_gap)) return false;
        prior_section=true;
    }
    if (presentation.is_fabricator && !presentation.recipe_inputs.empty()) {
        std::string inputs="Ingredients needed:";
        for (const auto& ingredient : presentation.recipe_inputs) inputs+="\n"+ingredient;
        if (!append_row(inputs,JUI::COLOUR_WHITE,section_gap)) return false;
    }
    if (presentation.is_fabricator && !presentation.recipe_output.empty() && !append_row("Produces: "+presentation.recipe_output,JUI::COLOUR_WHITE,section_gap)) return false;
    if (!has_killstreak) {
        if (!presentation.sheen.empty() && !append_coloured_value("Sheen: ",presentation.sheen,JUI::Colour(presentation.sheen_colour),section_gap)) return false;
        if (!presentation.killstreaker.empty() && !append_row("Killstreaker: "+presentation.killstreaker,JUI::COLOUR_WHITE,line_gap)) return false;
    }
    if (item_->is_renamed() && !presentation.decorated && !presentation.name.empty() && !append_row(presentation.name,JUI::COLOUR_WHITE,section_gap)) return false;
    uint32 crate_number=0;
    if (item_->get_crate_number(&crate_number) && !append_row("Crate Series #"+std::to_string(crate_number),JUI::COLOUR_WHITE,section_gap)) return false;
    const EItemToolType tool_type=item_->get_tool_type();
    if (tool_type==TOOL_DUEL_MINIGAME || tool_type==TOOL_NOISE_MAKER) {
        const uint32 count=item_->get_count();
        if (!append_row(count==FL_ITEM_UNLIMITED_COUNT ? "Uses: Unlimited" : "Uses: "+std::to_string(count),JUI::COLOUR_WHITE,section_gap)) return false;
    }
    if (presentation.bond_account_1) {
        const std::string name=presentation.bond_name_1.empty() ? "Steam user #"+std::to_string(presentation.bond_account_1) : presentation.bond_name_1;
        if (!append_row("A perpetual bond of commitment between "+name,JUI::Colour(0x8CC8F2),section_gap)) return false;
    }
    if (presentation.bond_account_2) {
        const std::string name=presentation.bond_name_2.empty() ? "Steam user #"+std::to_string(presentation.bond_account_2) : presentation.bond_name_2;
        if (!append_row("and "+name+", forever and ever and ever. XoXoX",JUI::Colour(0x8CC8F2),presentation.bond_account_1 ? line_gap : section_gap)) return false;
    }
    if (!presentation.restriction_note.empty() && !append_row("( "+presentation.restriction_note+" )",JUI::COLOUR_WHITE,section_gap)) return false;
    if (!presentation.custom_description.empty() && !append_row("\""+presentation.custom_description+"\"",JUI::COLOUR_WHITE,section_gap)) return false;
    if (presentation.gift_account) {
        const std::string sender=presentation.gift_name.empty() ? "Steam user #"+std::to_string(presentation.gift_account) : presentation.gift_name;
        if (!append_row("Gift from: "+sender,JUI::Colour(0x8CC8F2),section_gap)) return false;
        if (presentation.gift_timestamp) {
            time_t stamp=static_cast<time_t>(presentation.gift_timestamp);tm local={};char date[96]={};
            if (localtime_s(&local,&stamp)==0 && strftime(date,sizeof(date),"%B %d, %Y (%H:%M:%S)",&local))
                if (!append_row(std::string("Date Received: ")+date,JUI::COLOUR_WHITE,line_gap)) return false;
        }
    }
	pack();
    return true;
}

/*
 * Fade in/out based on activity.
 */
void ItemDisplay::update_alpha( void )
{
	const auto now=GetTickCount64();
	for (const auto& row : animated_spell_rows_) { const JUI::Colour colour(spell_paint_colour(row.second,now));row.first->set_colour(&colour); }
	const ULONGLONG elapsed=(std::min)(now-last_alpha_time_,static_cast<ULONGLONG>(200));
	last_alpha_time_=now;
	const float alpha_step=ITEM_DISPLAY_ALPHA_SPEED * static_cast<float>(elapsed) / 16.0f;

	if (is_active()) {
		// Increase and clamp alpha if active.
		float alpha = get_alpha() + alpha_step;
		if (alpha > ITEM_DISPLAY_ALPHA_MAX) {
			alpha = ITEM_DISPLAY_ALPHA_MAX;
		}
		set_alpha( alpha );
	}
	else {
		// Decrease alpha if inactive.
		set_alpha( get_alpha() - alpha_step );
	}
}

/*
 * Pack item display layout.
 */
void ItemDisplay::pack( void )
{
	text_layout_->set_constraint(name_text_,0,0);
	int height=name_text_->get_height();
	for (size_t i=0;i<detail_rows_.size();++i) {
		height+=detail_gaps_[i];
		JUI::Component* row=detail_rows_[i];
		text_layout_->set_constraint(row,(ITEM_DISPLAY_TEXT_WIDTH-row->get_width())/2,height);
		height+=row->get_height();
	}
	text_layout_->set_size(ITEM_DISPLAY_TEXT_WIDTH,height);
	RoundedRectangleContainer::pack();
}

/*
 * Get item shown by display.
 */
const Item* ItemDisplay::get_item( void ) const
{
	return item_;
}

/*
 * Set item to be described by display.
 * Returns true if display updated successfully, false otherwise.
 */
bool ItemDisplay::set_item( const Item *item )
{
	if ((item_ != item) || !is_active()) {
		item_ = item;
		if (item != nullptr) {
			set_active( true );
			return update_display();
		}
		else {
			set_active( false );
		}
	}

    return true;
}

/*
 * Set name of item to be displayed.
 */
void ItemDisplay::set_name( const JUTIL::WideString* name )
{
	name_text_->set_text( name );
}

/*
 * Return whether display is active.
 */
bool ItemDisplay::is_active( void ) const
{
	return is_active_;
}

/*
 * Set whether display is active.
 */
void ItemDisplay::set_active( bool is_active )
{
	is_active_ = is_active;
}

/*
 * Load item display resources.
 */
bool ItemDisplay::precache( void )
{
	// Font for item names.
	name_font_ = JUI::FontFactory::create_font( &ITEM_DISPLAY_TITLE_FONT_FACE, ITEM_DISPLAY_TITLE_FONT_SIZE );
	if (name_font_ == nullptr) {
		return false;
	}

	// Font for item description and miscellaneous information.
	info_font_ = JUI::FontFactory::create_font( ITEM_DISPLAY_INFO_FONT_FACE, ITEM_DISPLAY_INFO_FONT_SIZE );
	if (info_font_ == nullptr) {
		return false;
	}

    return true;
}
