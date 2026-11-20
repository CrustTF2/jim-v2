#include "item_decorator.hpp"
#include "spell_colours.hpp"

// Component parameters.
const unsigned int DECORATOR_WIDTH = 80;
const unsigned int DECORATOR_HEIGHT = 60;
const unsigned int ITEM_SIZE = 60;
const int BADGE_SIZE = 23;
const int BADGE_PADDING = 3;
const int BADGE_SPACING = 2;
const JUTIL::ConstantString STAT_CLOCK_BADGE("img/manager/badges/statclock.png");
const JUTIL::ConstantString STRANGE_BADGE("img/manager/badges/strange.png");
const JUTIL::ConstantString PAINT_BADGE("img/manager/badges/paint.png");
const JUTIL::ConstantString PAINT_FIRST_BADGE("img/manager/badges/paint_blu.png");
const JUTIL::ConstantString PAINT_SECOND_BADGE("img/manager/badges/paint_red.png");
const JUTIL::ConstantString PYROLAND_BADGE("img/manager/badges/pyroland.png");
const JUTIL::ConstantString HALLOWEEN_BADGE("img/manager/badges/halloween.png");
const JUTIL::ConstantString UNUSUAL_BADGE("img/manager/badges/unusual.png");

// Item decorator textures.
const JUTIL::ConstantString SPLAT_TEXTURE( "img/backpack/player/items/crafting/backpack_jewel_paint_splatter.png" );
const JUTIL::ConstantString SPLAT_FIRST_TEXTURE( "img/backpack/player/items/crafting/backpack_jewel_paint_splatter_blu.png" );
const JUTIL::ConstantString SPLAT_SECOND_TEXTURE( "img/backpack/player/items/crafting/backpack_jewel_paint_splatter_red.png" );
const JUTIL::ConstantString SPILL_TEXTURE( "img/backpack/player/items/crafting/paintcan_paintcolor.png" );
const JUTIL::ConstantString SPILL_FIRST_TEXTURE( "img/backpack/player/items/crafting/teampaint_blu_mask.png" );
const JUTIL::ConstantString SPILL_SECOND_TEXTURE( "img/backpack/player/items/crafting/teampaint_red_mask.png" );

// Item decorator fonts.
const JUTIL::ConstantString DECORATOR_FONT_FACE( "fonts/tf2build.ttf" );
const unsigned int DECORATOR_FONT_SIZE = 10;
const JUI::Colour* DECORATOR_FONT_COLOUR = &JUI::COLOUR_WHITE;
const JUI::Colour* DECORATOR_FONT_SHADOW_COLOUR = &JUI::COLOUR_BLACK;
const int CRATE_OFFSET = 5;
const int CRATE_SHADOW_OFFSET = 2;
JUI::Text* ItemDecorator::crate_text_ = nullptr;
JUI::Text* ItemDecorator::equipped_text_ = nullptr;

// Static texture pointers.
JUI::FileTexture* ItemDecorator::splat_ = nullptr;
JUI::FileTexture* ItemDecorator::splat_first_ = nullptr;
JUI::FileTexture* ItemDecorator::splat_second_ = nullptr;
JUI::FileTexture* ItemDecorator::spill_ = nullptr;
JUI::FileTexture* ItemDecorator::spill_first_ = nullptr;
JUI::FileTexture* ItemDecorator::spill_second_ = nullptr;
JUI::FileTexture* ItemDecorator::stat_clock_badge_ = nullptr;
JUI::FileTexture* ItemDecorator::strange_badge_ = nullptr;
JUI::FileTexture* ItemDecorator::unusual_badge_ = nullptr;
JUI::FileTexture* ItemDecorator::halloween_badge_ = nullptr;
JUI::FileTexture* ItemDecorator::pyroland_badge_ = nullptr;
JUI::FileTexture* ItemDecorator::paint_badge_ = nullptr;
JUI::FileTexture* ItemDecorator::paint_first_badge_ = nullptr;
JUI::FileTexture* ItemDecorator::paint_second_badge_ = nullptr;

// Hack since old Team Spirit doesn't send RGB in attributes.
const unsigned int TEAM_SPIRIT_COLOUR = 1;
const JUI::Colour TEAM_SPIRIT_FIRST( 0xB8383B, 1.0f );
const JUI::Colour TEAM_SPIRIT_SECOND( 0x5885A2, 1.0f );

/*
 * No item to start with.
 */
ItemDecorator::ItemDecorator( void )
	: item_( nullptr )
{
	set_size( DECORATOR_WIDTH, DECORATOR_HEIGHT );
}

/*
 * Destroy the item decorator.
 */
ItemDecorator::~ItemDecorator( void )
{
}

/*
 * Check/download the alternate item drawing images.
 */
bool ItemDecorator::download_resources( ResourceLoaderInterface* loader )
{
	if (!loader->get_resource( &SPLAT_TEXTURE, &SPLAT_TEXTURE )) {
		return false;
	}
	if (!loader->get_resource( &SPLAT_FIRST_TEXTURE, &SPLAT_FIRST_TEXTURE )) {
		return false;
	}
	if (!loader->get_resource( &SPLAT_SECOND_TEXTURE, &SPLAT_SECOND_TEXTURE )) {
		return false;
	}
	if (!loader->get_resource( &SPILL_TEXTURE, &SPILL_TEXTURE )) {
		return false;
	}
	if (!loader->get_resource( &SPILL_FIRST_TEXTURE, &SPILL_FIRST_TEXTURE )) {
		return false;
	}
	if (!loader->get_resource( &SPILL_SECOND_TEXTURE, &SPILL_SECOND_TEXTURE )) {
		return false;
	}
	return true;
}

/*
 * Load the textures.
 */
bool ItemDecorator::precache( JUI::Graphics2D* graphics )
{
	// Packaged locally from TF2; the original author's resource server has no badges.
	if (graphics->get_texture(&STAT_CLOCK_BADGE,&stat_clock_badge_) != JUI::Graphics2D::Success ||
		graphics->get_texture(&STRANGE_BADGE,&strange_badge_) != JUI::Graphics2D::Success ||
		graphics->get_texture(&UNUSUAL_BADGE,&unusual_badge_) != JUI::Graphics2D::Success ||
		graphics->get_texture(&HALLOWEEN_BADGE,&halloween_badge_) != JUI::Graphics2D::Success ||
		graphics->get_texture(&PYROLAND_BADGE,&pyroland_badge_) != JUI::Graphics2D::Success ||
		graphics->get_texture(&PAINT_BADGE,&paint_badge_) != JUI::Graphics2D::Success ||
		graphics->get_texture(&PAINT_FIRST_BADGE,&paint_first_badge_) != JUI::Graphics2D::Success ||
		graphics->get_texture(&PAINT_SECOND_BADGE,&paint_second_badge_) != JUI::Graphics2D::Success) return false;
	// Load all textures.
	JUI::Graphics2D::ReturnStatus status;
	status = graphics->get_texture( &SPLAT_TEXTURE, &splat_ );
	if (status != JUI::Graphics2D::Success) {
		return false;
	}
	status = graphics->get_texture( &SPLAT_FIRST_TEXTURE, &splat_first_ );
	if (status != JUI::Graphics2D::Success) {
		return false;
	}
	status = graphics->get_texture( &SPLAT_SECOND_TEXTURE, &splat_second_ );
	if (status != JUI::Graphics2D::Success) {
		return false;
	}
	status = graphics->get_texture( &SPILL_TEXTURE, &spill_ );
	if (status != JUI::Graphics2D::Success) {
		return false;
	}
	status = graphics->get_texture( &SPILL_FIRST_TEXTURE, &spill_first_ );
	if (status != JUI::Graphics2D::Success) {
		return false;
	}
	status = graphics->get_texture( &SPILL_SECOND_TEXTURE, &spill_second_ );
	if (status != JUI::Graphics2D::Success) {
		return false;
	}

	// Create font for rendering crate series and equipped.
	JUI::FontInterface* font = JUI::FontFactory::create_font( &DECORATOR_FONT_FACE, DECORATOR_FONT_SIZE );
	if (font == nullptr) {
		return false;
	}
	
	// Create crate and equipped text objects.
	if (!JUTIL::BaseAllocator::allocate( &crate_text_ )) {
		return false;
	}
	new (crate_text_) JUI::Text( font );
	if (!JUTIL::BaseAllocator::allocate( &equipped_text_ )) {
		return false;
	}
	new (equipped_text_) JUI::Text( font );
	equipped_text_->set_colour( DECORATOR_FONT_COLOUR );
	return true;
}

/*
 * All are textures, so nothing to clean for now.
 */
void ItemDecorator::release( void )
{
	JUTIL::BaseAllocator::safe_destroy( &crate_text_ );
	JUTIL::BaseAllocator::safe_destroy( &equipped_text_ );
}

/*
 * Set the item we're drawing.
 */
void ItemDecorator::set_item( const Item* item )
{
	item_ = item;
}

/*
 * Draw the item image and extras based on the type.
 */
void ItemDecorator::draw( JUI::Graphics2D* graphics )
{
	// Get coordinates first.
	graphics->push_matrix();
	graphics->translate( get_x(), get_y() );
	int item_x = (get_width() - ITEM_SIZE) / 2;
	int item_y = 0;

	// Draw splats underneath if this item is a non-coloured paint.
	if (item_ != nullptr) {
		const JUTIL::ConstantString SET_TINT_FIRST( "set item tint RGB" );
		const JUTIL::ConstantString SET_TINT_SECOND( "set item tint RGB 2" );
		bool is_paint_can = (item_->get_tool_type() == TOOL_PAINT_CAN);

		// Resolve colours first.
		const Attribute* first = item_->find_attribute( &SET_TINT_FIRST );
		bool has_first = (first != nullptr);
		bool has_second;
		JUI::Colour first_colour;
		JUI::Colour second_colour;
		if (first != nullptr) {
			const float* first_float = reinterpret_cast<const float*>(first->get_value());
			unsigned int first_value = static_cast<unsigned int>(*first_float);
			if (first_value == TEAM_SPIRIT_COLOUR) {
				first_colour = TEAM_SPIRIT_FIRST;
				second_colour = TEAM_SPIRIT_SECOND;
				has_second = true;
			}
			else {
				first_colour = JUI::Colour( first_value );
				const Attribute* second = item_->find_attribute( &SET_TINT_SECOND );
				has_second = (second != nullptr);
				if (has_second) {
					const float* second_float = reinterpret_cast<const float*>(second->get_value());
					unsigned int second_value = static_cast<unsigned int>(*second_float);
					second_colour = JUI::Colour( second_value );
				}
			}
		}

		// Draw the item.
		const JUI::Texture* texture = item_->get_texture();
		graphics->set_colour( &JUI::COLOUR_WHITE );
		if (texture != nullptr) {
			const int available_width = item_->has_thumbnail_texture() ? get_width()-6 : ITEM_SIZE;
			const int available_height = item_->has_thumbnail_texture() ? get_height() : ITEM_SIZE;
			const JUI::Viewport fitted = JUI::fit_viewport(texture->get_width(),texture->get_height(),available_width,available_height);
			const int image_x = item_->has_thumbnail_texture() ? 3 : item_x;
			graphics->draw_texture(texture,image_x+fitted.x,item_y+fitted.y,fitted.width,fitted.height);
		}

		// Valve's kit definition places its target icon inside the bottle.
		if (const JUI::Texture* target=item_->get_kit_target_texture()) {
			const int box=21;
			const JUI::Viewport fitted=JUI::fit_viewport(target->get_width(),target->get_height(),box,box);
			graphics->set_colour(&JUI::COLOUR_WHITE);
			graphics->draw_texture(target,item_x+17+fitted.x,item_y+26+fitted.y,fitted.width,fitted.height);
		}

		// Draw the spills above if it's a paint can.
		if (is_paint_can) {
			if (has_second) {
				graphics->set_colour( &first_colour );
				graphics->draw_texture( spill_first_, item_x, item_y, ITEM_SIZE, ITEM_SIZE );
				graphics->set_colour( &second_colour );
				graphics->draw_texture( spill_second_, item_x, item_y, ITEM_SIZE, ITEM_SIZE );
			}
			else {
				graphics->set_colour( &first_colour );
				graphics->draw_texture( spill_, item_x, item_y, ITEM_SIZE, ITEM_SIZE );
			}
		}

		// Draw crate series if this is a crate.
		uint32 crate_series;
		if (item_->get_crate_number( &crate_series )) {
			JUTIL::DynamicString series;
			if (series.write( "#%d", crate_series )) {
				// Draw shadow.
				crate_text_->set_colour( DECORATOR_FONT_SHADOW_COLOUR );
				crate_text_->set_text( &series );
				crate_text_->set_position( CRATE_OFFSET + CRATE_SHADOW_OFFSET, CRATE_OFFSET + CRATE_SHADOW_OFFSET );
				crate_text_->draw( graphics );

				// Draw text over shadow.
				crate_text_->set_colour( DECORATOR_FONT_COLOUR );
				crate_text_->set_text( &series );
				crate_text_->set_position( CRATE_OFFSET, CRATE_OFFSET );
				crate_text_->draw( graphics );
			}
		}
	}

	// Keep all indicators together, with paint at the top as in TF2.
	if (item_ != nullptr) {
		const JUTIL::ConstantString first_name("set item tint RGB");
		const JUTIL::ConstantString second_name("set item tint RGB 2");
		const Attribute* first = item_->find_attribute(&first_name);
		const int spell_index = item_->get_presentation().paint_spell_index;
		const bool paint = (first || spell_index>=0) && item_->get_tool_type() != TOOL_PAINT_CAN;
		const auto badges = item_->get_presentation().badges();
		const bool decal = item_->get_decal_texture() != nullptr;
		const int count = static_cast<int>(badges.size()) + (paint ? 1 : 0) + (decal ? 1 : 0);
		const int available = get_height()-2*BADGE_PADDING+(count > 0 ? count-1 : 0)*2;
		const int size = count > 0 ? (std::min)(BADGE_SIZE,available/count) : BADGE_SIZE;
		int badge_y = BADGE_PADDING, badge_x = get_width()-BADGE_PADDING-size;
		const int badge_step = count > 2 ? size-2 : size+BADGE_SPACING;
		if (paint) {
			const unsigned int value = spell_index>=0 ? spell_paint_colour(static_cast<unsigned int>(spell_index),GetTickCount64()) : static_cast<unsigned int>(*reinterpret_cast<const float*>(first->get_value()));
			const Attribute* second = spell_index>=0 ? nullptr : item_->find_attribute(&second_name);
			if (spell_index<0 && (value == TEAM_SPIRIT_COLOUR || second)) {
				const JUI::Colour one = value == TEAM_SPIRIT_COLOUR ? TEAM_SPIRIT_FIRST : JUI::Colour(value);
				const JUI::Colour two = value == TEAM_SPIRIT_COLOUR ? TEAM_SPIRIT_SECOND : JUI::Colour(static_cast<unsigned int>(*reinterpret_cast<const float*>(second->get_value())));
				graphics->set_colour(&one);graphics->draw_texture(paint_first_badge_,badge_x,badge_y,size,size);
				graphics->set_colour(&two);graphics->draw_texture(paint_second_badge_,badge_x,badge_y,size,size);
			} else {
				const JUI::Colour color(value);graphics->set_colour(&color);graphics->draw_texture(paint_badge_,badge_x,badge_y,size,size);
			}
			badge_y += badge_step;
		}
		if (decal) {
			graphics->set_colour(&JUI::COLOUR_WHITE);
			graphics->draw_texture(item_->get_decal_texture(),badge_x,badge_y,size,size);
			badge_y += badge_step;
		}
		for (ItemBadge badge : badges) {
			JUI::FileTexture* texture = badge == ItemBadge::Halloween ? halloween_badge_ :
				badge == ItemBadge::Pyroland ? pyroland_badge_ : badge == ItemBadge::Unusual ? unusual_badge_ : badge == ItemBadge::StatClock ? stat_clock_badge_ : strange_badge_;
			graphics->set_colour(&JUI::COLOUR_WHITE);
			graphics->draw_texture(texture,badge_x,badge_y,size,size);
			badge_y += badge_step;
		}
	}
	graphics->pop_matrix();
}
