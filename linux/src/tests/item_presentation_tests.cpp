#include "item_presentation.hpp"
#include "item.hpp"
#include "protobuf/tf_gcmessages.pb.h"
#include "spell_colours.hpp"
#include "selection_bounds.hpp"
#include "frame_limiter.hpp"
#include <jui/application/viewport.hpp>
#include <iostream>
#include <limits>
#include <cstring>
#include <cstdlib>

int checks = 0;
void require(bool condition, const char* message) {
    ++checks;
    if (!condition) { std::cerr << "FAILED: " << message << '\n'; std::exit(1); }
}
template<class T> PresentationAttribute attr(unsigned int id, T number) {
    PresentationAttribute result{id,std::vector<char>(sizeof(number))};
    std::memcpy(result.bytes.data(),&number,sizeof(number));
    return result;
}
int main(int argc, char** argv) {
    require(argc==2,"metadata path supplied");
    ItemPresentationSchema schema;
    require(schema.load(argv[1]),"load packaged game metadata");
    auto hat=schema.resolve(30000,5,{attr(134,13.0f)},"Hat");
    require(hat.unusual && hat.effects=="Unusual Effect: Burning Flames","hat float effect ID");
    auto taunt=schema.resolve(0,5,{attr(2041,std::uint32_t(3001))},"Taunt");
    require(taunt.effects=="Unusual Effect: Showstopper","taunt integer effect ID");
    auto night=schema.resolve(15000,15,{},"Decorated Weapon Sniper Rifle");
    require(night.name=="Night Owl Sniper Rifle" && night.grade==3,"legacy skin defaults and collection grade");
    auto woodland=schema.resolve(15006,15,{},"Rocket Launcher");
    require(woodland.name=="Woodland Warrior Rocket Launcher" && woodland.grade==4,"Commando legacy skin");
    require(schema.resolve(15003,15,{},"Decorated Weapon Shotgun").name=="Backwoods Boomstick Shotgun","shotgun localization fallback");
    auto paint=schema.resolve(9536,15,{attr(834,std::uint32_t(390))},"War Paint");
    require(paint.name=="Dragon Slayer War Paint" && paint.grade==6,"unapplied dynamic War Paint");
    auto applied=schema.resolve(15006,15,{attr(834,std::uint32_t(390)),attr(134,702.0f),attr(189,11.0f),attr(214,std::uint32_t(0)),attr(725,0.2f)},"Rocket Launcher");
    require(applied.name=="Dragon Slayer Rocket Launcher" && applied.grade==6,"instance paint replaces fixed paint and rarity");
    require(applied.unusual && applied.strange,"Strange and Unusual coexist with Decorated quality");
    require(applied.effects=="Unusual Effect: Isotope","Unusual weapon effect name");
    require(applied.wear=="Factory New","TF2 wear boundary (not CS wear ranges)");
    require(applied.paint_id==390 && applied.has_wear && applied.wear_value==0.2f,"instance paint and wear passed to thumbnail renderer");
    require(woodland.paint_id==6 && woodland.has_wear && woodland.wear_value==0.6f,"legacy paint and default wear passed to renderer");
    const char* wear_names[]={"Factory New","Minimal Wear","Field-Tested","Well-Worn","Battle Scarred"};
    const float wear_values[]={0.2f,0.4f,0.6f,0.8f,1.0f};
    for(int i=0;i<5;++i) require(schema.resolve(15000,15,{attr(725,wear_values[i])},"Sniper Rifle").wear==wear_names[i],"all five wear boundaries");
    auto invalid=schema.resolve(15000,15,{{834,{'x'}},attr(134,std::numeric_limits<float>::quiet_NaN())},"Sniper Rifle");
    require(invalid.name==night.name && !invalid.unusual,"truncated and NaN attributes ignored");
    auto unknown=schema.resolve(9536,15,{attr(834,std::uint32_t(99999)),attr(134,99999.0f)},"War Paint");
    require(unknown.name=="Unknown War Paint #99999 War Paint","unknown paint remains identifiable");
    require(unknown.effects=="Unusual Effect: Unknown effect #99999","unknown effect remains identifiable");
    require(schema.resolve(15000,15,{attr(834,std::uint32_t(99999))},"Sniper Rifle").grade==0,"unknown replacement paint does not inherit unrelated grade");
    auto painted=schema.resolve(30000,6,{attr(142,7511618.0f)},"Cosmetic");
    require(painted.paint_colour_name=="Indubitably Green" && painted.paint_colour_rgb==7511618,"paint color is resolved from TF2 paint definitions");
    auto team_paint=schema.resolve(30000,11,{attr(142,1.0f),attr(214,std::uint32_t(5))},"Cosmetic");
    require(team_paint.paint_colour_name=="Team Spirit" && team_paint.paint_colour_rgb==0xB8383B && team_paint.strange,"legacy Team Spirit is resolved for Strange items");
    auto killed=schema.resolve(18,6,{attr(2025,2.0f),attr(2014,5.0f)},"Rocket Launcher");
    require(killed.killstreak_tier==2 && killed.killstreak_prefix=="Specialized Killstreak" && killed.sheen=="Agonizing Emerald" && killed.sheen_colour==0x28FF46,"applied specialized killstreak and game sheen color");
    auto killed_pro=schema.resolve(18,6,{attr(2025,3.0f),attr(2014,2.0f),attr(2013,2002.0f)},"Rocket Launcher");
    require(killed_pro.killstreak_prefix=="Professional Killstreak" && killed_pro.killstreaker=="Fire Horns" && killed_pro.sheen_colour==0xF2AC0A,"professional killstreak effects and sheen color");
    auto intrinsic=schema.resolve(11611,6,{},"Medal");
    require(intrinsic.paint_colour_name.empty(),"built-in medal tint is not reported as player-applied paint");
    auto not_painted=schema.resolve(5027,6,{},"Paint Can");
    require(not_painted.paint_colour_name.empty(),"paint cans are not described as painted items");
    auto basic_kit=schema.resolve(5728,6,{},"Kit");
    require(basic_kit.is_killstreak_kit && basic_kit.kit_target==201 && basic_kit.name=="Killstreak Sniper Rifle Kit","fixed target basic kit uses schema defaults");
    auto kit=schema.resolve(6523,6,{attr(2012,200.0f),attr(2014,6.0f)},"Kit");
    require(kit.name=="Specialized Killstreak Scattergun Kit" && kit.kit_target==200 && kit.sheen=="Villainous Violet","instance target and sheen on specialized kit");
    auto pro=schema.resolve(6526,6,{attr(2012,18.0f),attr(2014,2.0f),attr(2013,2002.0f)},"Kit");
    require(pro.name=="Professional Killstreak Rocket Launcher Kit" && pro.sheen=="Deadly Daffodil" && pro.killstreaker=="Fire Horns","professional kit resolves both effects");
    auto component=[](unsigned int id,unsigned int flags,unsigned int definition,unsigned int quality,unsigned int count,unsigned int used,const std::string& encoded) {
        CAttribute_DynamicRecipeComponent msg;msg.set_component_flags(flags);msg.set_def_index(definition);msg.set_item_quality(quality);msg.set_num_required(count);msg.set_num_fulfilled(used);msg.set_attributes_string(encoded);
        const std::string bytes=msg.SerializeAsString();return PresentationAttribute{id,std::vector<char>(bytes.begin(),bytes.end())};
    };
    const std::string separator="|\x01\x02\x01\x03|\x01\x02\x01\x03|";
    auto fabricator=schema.resolve(20002,6,{
        component(2000,8|16,0,6,1,0,"2025"+separator+"1"),
        component(2001,4,5706,6,11,2,""),
        component(2002,1|4,6523,6,1,0,"2012"+separator+"200"+separator+"2014"+separator+"6")},"Fabricator");
    require(fabricator.is_fabricator && fabricator.name=="Specialized Killstreak Scattergun Kit Fabricator" && fabricator.recipe_output=="Specialized Killstreak Scattergun Kit","fabricator names its encoded output");
    require(fabricator.sheen=="Villainous Violet" && fabricator.recipe_inputs.size()==2 && fabricator.recipe_inputs[0]=="Unique Killstreak Item x 1" && fabricator.recipe_inputs[1]=="Battle-Worn Robot KB-808 x 9","fabricator uses encoded requirements and outstanding quantity");
    auto pro_fabricator=schema.resolve(20003,6,{
        component(2002,1|4,6526,6,1,0,"2012"+separator+"18"+separator+"2014"+separator+"2"+separator+"2013"+separator+"2002"),
        component(2003,4,5706,6,5,5,"")},"Fabricator");
    require(pro_fabricator.is_fabricator && pro_fabricator.name=="Professional Killstreak Rocket Launcher Kit Fabricator" && pro_fabricator.sheen=="Deadly Daffodil" && pro_fabricator.killstreaker=="Fire Horns" && pro_fabricator.recipe_inputs.empty(),"professional fabricator output effects and completed inputs");
    auto bad_recipe=schema.resolve(20002,6,{{2000,{'x'}}},"Fabricator");
    require(!bad_recipe.is_fabricator && bad_recipe.recipe_inputs.empty(),"malformed recipe is ignored safely");
    auto normal=schema.resolve(18,6,{},"Rocket Launcher");
    require(normal.grade==0 && !normal.decorated && !normal.unusual && normal.name.empty(),"ordinary unique weapon unchanged");
    auto no_effect=schema.resolve(18,6,{attr(134,0.0f)},"Rocket Launcher");
    require(!no_effect.unusual,"zero particle ID is not Unusual");
    require(ItemPresentation::grade_colour(3)==0x4B69FF && ItemPresentation::grade_colour(4)==0x8847FF &&
            ItemPresentation::grade_colour(5)==0xD32CE6 && ItemPresentation::grade_colour(6)==0xEB4B4B,"requested border colours");
    require(std::string(ItemPresentation::grade_name(5))=="Assassin Grade","hover grade label");
    ItemPresentationSchema missing;
    require(!missing.load("file-that-does-not-exist.json"),"missing metadata detected");
    auto fallback=missing.resolve(18,6,{},"Rocket Launcher");
    require(!fallback.decorated && fallback.grade==0,"missing metadata preserves ordinary items");
    auto future=missing.resolve(9999,15,{attr(834,std::uint32_t(99999))},"Decorated Weapon Rocket Launcher");
    require(future.name=="Unknown War Paint #99999 Rocket Launcher","legacy prefix stripped in fallback");
    for (unsigned int grade=3;grade<=6;++grade) {
        // Graded cosmetics use collection rarity just like painted weapons.
        const unsigned int cosmetic[]={30623,30625,30621,30640};
        require(schema.resolve(cosmetic[grade-3],6,{},"Cosmetic").grade==grade,"cosmetic grade resolved");
    }
    JUTIL::DynamicString image;
    JUI::FileTexture* copied_filename=nullptr;
    { JUTIL::DynamicString transient;transient.write("temporary-thumbnail.png");copied_filename=new JUI::FileTexture(&transient); }
    require(std::string(copied_filename->get_filename()->get_string())=="temporary-thumbnail.png","texture retains a filename after the input string is destroyed");
    delete copied_filename;
    auto* base_name=new JUTIL::DynamicString();
    base_name->write("Rocket Launcher");
    ItemDefinition definition(base_name,&image,0);
    Item item(1,15006,1,static_cast<EItemQuality>(15),1,0,0,0);
    item.set_original_id(10008628290ULL);
    require(item.get_paint_seed()==10008628290ULL,"original 64-bit item ID is the default paint seed");
    auto* lo=new Attribute(866);auto* hi=new Attribute(867);
    std::uint32_t seedlo=0x87654321,seedhi=0x12345678;
    require(lo->set_value(reinterpret_cast<const char*>(&seedlo),sizeof(seedlo)) && item.add_attribute(lo),"low seed attribute accepted");
    require(item.get_paint_seed()==10008628290ULL,"incomplete seed override preserves original ID");
    require(hi->set_value(reinterpret_cast<const char*>(&seedhi),sizeof(seedhi)) && item.add_attribute(hi),"high seed attribute accepted");
    require(item.get_paint_seed()==0x1234567887654321ULL,"both explicit seed halves override original ID");
    item.set_definition(&definition);
    item.set_presentation(killed);
    require(item.generate_name() && std::string(item.get_name()->get_string())=="Specialized Killstreak Rocket Launcher","applied killstreak reaches the item name");
    item.set_presentation(applied);
    require(item.generate_name() && std::string(item.get_name()->get_string())=="Dragon Slayer Rocket Launcher","resolved name reaches Item model");
    JUTIL::ConstantString rename("My launcher");
    require(item.set_custom_name(&rename) && item.is_renamed() && std::string(item.get_name()->get_string())=="\"My launcher\"","custom name retained without appending generated name");
    require(item.get_presentation().finish=="Dragon Slayer" && item.get_presentation().effects.find("Isotope")!=std::string::npos,"renaming keeps finish and effect details");
    require(item.get_border_colour()->get_red()>0.9f && item.get_quality_colour()->get_red()>0.9f,"Elite grade colors both the border and Unusual title");
    require(item.generate_name() && item.generate_name() && !item.is_renamed() && std::string(item.get_name()->get_string())=="Dragon Slayer Rocket Launcher","repeated resolution does not duplicate names");

    auto gold=schema.resolve(197,11,{attr(2027,std::uint32_t(1)),attr(228,std::uint32_t(12345)),attr(2053,1.0f)},"Wrench",3,0);
    require(gold.australium && gold.australium_image.find("w_wrench_gold")!=std::string::npos && gold.crafter_account==12345 && gold.festivized,"Australium art, crafter, and Festivized attributes resolve together");
    item.set_presentation(gold);
    require(item.generate_name() && std::string(item.get_name()->get_string())=="Australium Rocket Launcher","Australium is included in the generated name");
    auto mixed_spells=schema.resolve(18,6,{attr(1004,0.0f),attr(1007,1.0f),attr(1008,1.0f),attr(1009,1.0f)},"Weapon");
    require(mixed_spells.spells.size()==4 && mixed_spells.spells[0].paint_index==0 && mixed_spells.spells[1].name=="Pumpkin Bombs" && mixed_spells.spells[1].colour==0xFF8C00 && mixed_spells.spells[2].name=="Halloween Fire" && mixed_spells.spells[2].colour==0x32CD32 && mixed_spells.spells[3].colour==0xA680B8,"simultaneous spells retain independent colors and animation");
    require(schema.resolve(205,6,{attr(2028,1.0f)},"Weapon",3,2).restriction_note=="Not Usable in Crafting","craft-only restriction");
    require(schema.resolve(205,6,{attr(785,std::uint32_t(1))},"Weapon",3,3).restriction_note=="Not Tradable, Marketable, Usable in Crafting, or Gift Wrappable","combined item restrictions");
    require(schema.resolve(205,6,{},"Weapon",1,0).restriction_note=="Achievement Item: Not Tradable or Marketable","achievement restriction");
    require(schema.resolve(205,6,{},"Weapon",29,0).restriction_note=="Free Contract Reward: Not Tradable or Marketable","contract restriction");
    require(schema.resolve(205,6,{},"Weapon",17,0).restriction_note=="Test Run - Cannot be traded, marketed, crafted, or modified","test run restriction");
    require(schema.resolve(205,6,{},"Weapon",24,0).restriction_note=="Loaner - Cannot be traded, marketed, crafted, or modified","loaner restriction");
    require(schema.resolve(205,6,{attr(195,1.0f),attr(2028,1.0f)},"Weapon",3,3).restriction_note.empty(),"always-tradable schema exception");
    require(schema.resolve(205,6,{},"Rocket Launcher",0,0).restriction_note.empty(),"ordinary Unique weapon has no guessed marketability tag");
    require(schema.resolve(30000,6,{},"Cosmetic",3,0).restriction_note.empty(),"ordinary Unique cosmetic has no guessed marketability tag");
    require(schema.resolve(205,6,{},"Rocket Launcher",3,1).restriction_note=="Not Tradable or Marketable","explicit untradable flag is retained");
    require(schema.resolve(205,6,{},"Rocket Launcher",3,2).restriction_note=="Not Usable in Crafting","explicit uncraftable flag is retained");
    require(schema.resolve(6523,6,{},"Kit",3,0).restriction_note.empty(),"schema-only craft defaults do not invent an inventory tag");
    for (unsigned int definition : {343u,699u,726u,870u,871u,975u,1066u,5074u,5075u}) {
        require(schema.resolve(definition,6,{},"Granted item",3,0).restriction_note=="Not Tradable or Marketable","definition-level cannot-trade attribute is honored for granted items");
    }
    require(schema.resolve(343,11,{attr(214,std::uint32_t(0))},"Professor Speks",3,0).restriction_note=="Not Tradable or Marketable","Strange Professor Speks keeps its restriction");
    require(schema.resolve(343,6,{attr(195,1.0f)},"Professor Speks",3,0).restriction_note.empty(),"explicit always-tradable attribute overrides the definition restriction");
    auto bond=schema.resolve(5075,6,{attr(372,std::uint32_t(123456)),attr(373,std::uint32_t(654321))},"Ring",3,0);
    require(bond.bond_account_1==123456 && bond.bond_account_2==654321 && bond.restriction_note=="Not Tradable or Marketable","ring resolves both integer Steam account identifiers and its restriction");
    require(schema.resolve(5075,6,{{372,{'x'}}},"Ring",3,0).bond_account_1==0,"invalid bond account attribute is ignored");
    auto traveler=schema.resolve(1899,6,{},"World Traveler's Hat",3,0);
    require(traveler.force_unique && !traveler.unusual && !traveler.strange && !traveler.effects.empty() && traveler.badges().empty(),"World Traveler retains its built-in effect without Unusual quality or badge");
    require(traveler.restriction_note=="Not Tradable, Marketable, or Usable in Crafting","World Traveler's full restriction tag");
    auto traveler_unusual=schema.resolve(1899,5,{},"World Traveler's Hat",3,0);
    require(traveler_unusual.force_unique && !traveler_unusual.unusual && !traveler_unusual.strange && !traveler_unusual.effects.empty(),"World Traveler remains Unique even with an unexpected quality value");
    Item traveler_item(2,1899,20,k_EItemQuality_Unusual,1,0,0,3);
    traveler_item.set_presentation(traveler_unusual);
    require(traveler_item.get_quality_colour()->get_red()>0.99f && traveler_item.get_quality_colour()->get_green()>0.8f && traveler_item.get_border_colour()->get_red()>0.99f,"World Traveler uses the Unique name and border color");
    require(highest_refresh_rate(highest_refresh_rate(0,60),144)==144 && highest_refresh_rate(144,60)==144 && highest_refresh_rate(144,0)==144 && highest_refresh_rate(0,50)==50,"highest valid active monitor refresh is selected");
    require(can_start_selection_box(true,0) && !can_start_selection_box(false,0) && !can_start_selection_box(true,1),"selection box requires left Shift and an empty selection");
    const auto rectangle=SelectionBounds::from_points(100,100,20,30);
    require(rectangle.left==20 && rectangle.top==30 && rectangle.right==100 && rectangle.bottom==100,"selection works when dragged in reverse");
    require(rectangle.intersects(95,95,20,20) && rectangle.intersects(30,40,10,10) && !rectangle.intersects(120,120,10,10),"selection includes intersecting and contained slots only");
    auto pyro=schema.resolve(738,6,{},"Balloonicorn");
    require(pyro.pyrovision_restricted && pyro.badges()==std::vector<ItemBadge>{ItemBadge::Pyroland},"Pyroland restriction is resolved from game metadata");
    CAttribute_String description;description.set_value("A custom description");
    const std::string desc=description.SerializeAsString();
    auto gifted=schema.resolve(18,6,{{501,std::vector<char>(desc.begin(),desc.end())},attr(185,std::uint32_t(1776124800)),attr(186,std::uint32_t(12345))},"Rocket Launcher");
    require(gifted.custom_description=="A custom description" && gifted.gift_account==12345 && gifted.gift_timestamp==1776124800,"protobuf description and gift metadata are decoded");
    auto paint_spell=schema.resolve(18,6,{attr(1004,0.0f),attr(1005,8421376.0f),attr(1006,1.0f)},"Rocket Launcher");
    require(paint_spell.paint_spell_index==0 && paint_spell.spells.size()==3 && paint_spell.spells[0].name=="Die Job" && paint_spell.spells[1].name=="Gangreen Footprints" && paint_spell.spells[1].colour==8421376 && paint_spell.spells[2].name=="Voices from Below","zero-valued paint spell and footprint spells resolved");
    require(spell_paint_colour(0,0)==SPELL_PAINT_SAMPLES[0][0] && spell_paint_colour(0,0)!=spell_paint_colour(0,1000) && spell_paint_colour(0,1000)==spell_paint_colour(0,1000),"spell animation uses shared TF2 color samples");
    for (int i=0;i<5;++i) require(schema.resolve(18,6,{attr(1004,static_cast<float>(i))},"Weapon").paint_spell_index==i,"all five paint spells resolved");
    for (unsigned int id : {1u,2u,8421376u,3100495u,5322826u,13595446u,8208497u}) {
        auto p=schema.resolve(18,6,{attr(1005,static_cast<float>(id))},"Weapon");
        require(p.spells.size()==1 && !p.spells[0].name.empty(),"all footprint spell names resolved");
    }
    auto halloween=schema.resolve(115,6,{},"Halloween Mask");
    require(halloween.halloween_restricted && halloween.badges()==std::vector<ItemBadge>{ItemBadge::Halloween},"Halloween-restricted item has the game badge");
    auto decal=schema.resolve(474,6,{attr(152,std::uint32_t(0x87654321)),attr(227,std::uint32_t(0x12345678))},"Objector");
    require(decal.decal_handle==0x1234567887654321ULL,"decal handle combines both Steam Cloud attribute halves");
    require(schema.resolve(474,6,{attr(152,std::uint32_t(42))},"Objector").decal_handle==0,"partial decal handle is not downloaded");
    require(normal.badges().empty(),"ordinary items have no quality badges");
    require(hat.badges()==std::vector<ItemBadge>{ItemBadge::Unusual},"Unusual-only badge");
    auto strange=schema.resolve(18,11,{attr(214,std::uint32_t(0))},"Rocket Launcher");
    require(strange.badges()==std::vector<ItemBadge>{ItemBadge::Strange},"ordinary Strange weapon badge");
    require(applied.badges()==std::vector<ItemBadge>{ItemBadge::Unusual,ItemBadge::StatClock},"Unusual above Stat-Clock, without duplicate Strange badge");
    auto painted_strange=schema.resolve(15006,15,{attr(214,std::uint32_t(0))},"Rocket Launcher");
    require(painted_strange.strange && painted_strange.stat_clock && painted_strange.badges()==std::vector<ItemBadge>{ItemBadge::StatClock},"zero-kill painted weapon is Stat-Clock");
    require(paint.badges().empty(),"ordinary War Paint is not Stat-Clock merely because it supports one");
    auto strange_paint=schema.resolve(9536,15,{attr(834,std::uint32_t(390)),attr(214,std::uint32_t(0))},"War Paint");
    require(strange_paint.badges()==std::vector<ItemBadge>{ItemBadge::StatClock},"unapplied Strange War Paint Stat-Clock");
    for (unsigned int score : {214u,294u,379u,381u,383u}) {
        auto special=schema.resolve(30000,5,{attr(134,13.0f),attr(score,std::uint32_t(0))},"Hat");
        require(special.badges()==std::vector<ItemBadge>{ItemBadge::Unusual,ItemBadge::Strange},"all five Strange counters stack with Unusual");
    }
    require(!schema.resolve(18,6,{attr(80,std::uint32_t(1))},"Rocket Launcher").strange,"unrelated attribute is not a Strange counter");
    auto hd=JUI::fit_viewport(960,540,1280,720);
    require(hd.x==0 && hd.y==0 && hd.width==1280 && hd.height==720,"720p fills the default window");
    require(hd.to_logical_x(640)==480 && hd.to_logical_y(360)==270,"720p mouse maps to UI center");
    auto wide=JUI::fit_viewport(960,540,1600,720);
    require(wide.x==160 && wide.y==0 && wide.width==1280 && wide.height==720,"wide window has centered side margins");
    require(!wide.contains(100,360) && wide.contains(800,360) && wide.to_logical_x(800)==480,"side margins cannot hit item slots");
    auto tall=JUI::fit_viewport(960,540,960,900);
    require(tall.x==0 && tall.y==180 && tall.height==540 && tall.to_logical_y(450)==270,"tall window maps vertical margins");
    auto minimized=JUI::fit_viewport(960,540,0,0);
    require(!minimized.contains(0,0) && minimized.to_logical_x(10)==-1,"minimizing does not divide by zero");
    for (auto size : {std::pair<int,int>{960,540},{1280,720},{1920,1080},{2560,1440},{1600,900},{1101,701},{960,1000}}) {
        const auto view=JUI::fit_viewport(960,540,size.first,size.second);
        require(std::abs(view.width/960.0-view.height/540.0)<0.002,"aspect ratio retained at resize sizes");
        bool centers_match=true;
        for(int row=0;row<5;++row) for(int column=0;column<10;++column) {
            const int x=98+column*85,y=65+row*65;
            const int px=view.x+static_cast<int>(std::round(x*view.width/960.0));
            const int py=view.y+static_cast<int>(std::round(y*view.height/540.0));
            centers_match=centers_match && view.contains(px,py) && std::abs(view.to_logical_x(px)-x)<=1 && std::abs(view.to_logical_y(py)-y)<=1;
        }
        require(centers_match,"all fifty slot centers map back correctly after resize");
    }
    const auto rectangular_icon=JUI::fit_viewport(128,82,60,60);
    require(rectangular_icon.width==60 && rectangular_icon.height==38 && rectangular_icon.y==11,"non-square item images are fitted, not stretched");
    const auto generated_icon=JUI::fit_viewport(256,160,74,60);
    require(generated_icon.width==74 && generated_icon.height==46 && generated_icon.y==7,"generated thumbnails fill the slot while preserving aspect ratio");
    JUI::Window window(nullptr);
    window.set_size(960,540);
    window.set_initial_client_size(1280,720);
    require(window.get_width()==960 && window.get_viewport().width==1280,"window separates logical layout from initial client pixels without opening a window");
    std::cout << checks << " item presentation and scaling checks passed\n";
}
