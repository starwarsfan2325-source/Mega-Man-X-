#include "editor.hpp"
#include "xgui/editor_w.hpp"
#include "input.hpp"
#include "event.hpp"
#include <fstream>
#include <filesystem>

namespace xpp::editor {

namespace {

int tile_mousex = 0;
int tile_mousey = 0;
video::texture grid_texture;

void
draw_grid(void)
{
	/*video::set_color(0.0f, 0.3f, 0.3f);
	int sx = (scrollx / 16) * 16;
	int sy = (scrolly / 16) * 16;
	for (int y = 0; y < 64; ++y) {
		video::draw_line(sx, sy + (y * 16), sx + 640, sy + (y * 16));
		for(int x = 0; x < 64; ++x) {
			video::draw_line(sx + (x * 16), sy, sx + (x * 16), sy + 640);
		}
	}
	video::flush_lines();*/
	for (int y = 0; y < tiles->map_width * 16; y += 16 * 14) {
		for (int x = 0; x < tiles->map_height * 16; x += 16 * 16) {
			video::draw_sprite(grid_texture, x, y);
		}
	}
}

void
draw_objects(void)
{
	int n = 0;
	for (auto &i : level.objects) {
		if (object_list.find(i.id) == object_list.end())
			continue; // TODO: show empty objects

		auto &entry = object_list[i.id];
		video::draw_sprite(entry.level_preview, i.x, i.y, i.flip);
		int ow = entry.level_preview.width;
		int oh = entry.level_preview.height;
		if (selected_object == n && mode == MODE_OBJECT) {
			video::set_color(1.f, .9f, 0.f);
			video::draw_rect_line(i.x - 2, i.y - 2, ow + 4, oh + 4);
		}

		++n;
	}
}

// TODO: line from last tile place holding shift like gimp
void
mouse_press_tile(void)
{
	if (selected_tile == -1)
		return;

	// drag?
}

void
mouse_press_object(void)
{
	if (selected_object_entry == "empty")
		return;

	place_object(selected_object_entry, tile_mousex * 16, tile_mousey * 16);
}

void
on_mouse_press(int button)
{
	if (!mouse_in_canvas())
		return;

	if (mode == MODE_TILE) {
		mouse_press_tile();
	} else if (mode == MODE_OBJECT) {
		mouse_press_object();
	}
}

} // namespace

// interface

level_t level;
std::unordered_map<std::string, object_entry_t> object_list;
std::string selected_object_entry = "empty";
int selected_object = -1;
int selected_tile = -1;
int selected_tile_max = -1;
int selected_level_tile = -1;
int selected_level_tile_max = -1;
int mousex = 0;
int mousey = 0;
int scroll_mousex = 0;
int scroll_mousey = 0;
int scrollx = 0;
int scrolly = 0;
int canvas_window_width = 1;
int canvas_window_height = 1;
int mode = MODE_TILE;
uint8_t attrib = 0;
bool selecting_block = false;
tile_block_t block_clipboard;
std::vector<tile_block_t> stamps;
video::tilemap *tiles;

/*

TODO:
alt stanps random
{rite clck menu save Stamps (programmable? randomness?) lua teli cijjluseikn
direct stamp editor, up to 256 tile collision type/functions,
stamp can turn on/off things like associated collision type when placing
stamp auto tiling script and editor including flip and stride

have a different mode for stamp auto-tiling, stamp size snap

tile + stamp add list, rewrite quad shader uv calc, screen 16*14 system

*/

void
init(void)
{
	event::subscribe<int>("editor.canvas_click", on_mouse_press);

	auto &_x = add_object_entry("x", "Mega Man X", "res/x.png")
		.level_preview = video::load_texture("res/ching01.png");
	auto &_scriver = add_object_entry("scriver", "Scriver", "res/scriver.png")
		.level_preview = video::load_texture("res/scriver_level.png");

	auto &_capsule = add_object_entry("capsule", "Capsule", "res/capsule.png");
	_capsule.level_preview = video::load_texture("res/capsule_level.png");
	_capsule.add_property_combo("Armor type", "Helmet (X1)\0Helmet (X2)\0Helmet (X3)\0");
	_capsule.add_property_bool("Showcase ability");
	_capsule.add_property_bool("Enable dialogue", true);
	_capsule.add_property_string("Dialogue", "dialogue.get_chest");

	place_object("capsule", 32, 32);

	selected_object = 0;

	grid_texture = video::load_texture("res/grid.png");
	tiles = new video::tilemap();
	//tiles->init(video::load_texture("res/seahorse.png"), 16 * 1, 14 * 1);
	//tiles->init("res/gfx/demo/bg01.png", 16 * 1, 14 * 1);
	tiles->init("res/seahorse.png", 16 * 1, 14 * 1);
}

bool
mouse_in_canvas(void)
{
	if (mousex >= 0 && mousey >= 0 &&
		mousex < canvas_window_width &&
		mousey < canvas_window_height)
		return true;
	else
		return false;
}


object_entry_t &
add_object_entry(std::string id, std::string name, video::texture preview)
{
	if (object_list.find(id) != object_list.end()) {
		printx(CONSOLE_WARN, "Already added object entry '%s', ignoring...", id.c_str());
		return object_list[id];
	}
	object_entry_t obj;
	obj.preview = preview;
	obj.level_preview = preview;
	obj.name = name;
	obj.id = id;
	object_list[id] = std::move(obj);
	return object_list[id];
}

// TODO: use texture id instead of file?
object_entry_t &
add_object_entry(std::string id, std::string name, std::string preview_file)
{
	return add_object_entry(id, name, video::load_texture(preview_file));
}

void
place_object(std::string id, int x, int y)
{
	if (object_list.find(id) == object_list.end())
		return;

	auto &entry = object_list[id];
	level_object_t obj;
	obj.properties = entry.properties; // copy
	obj.id = id;
	obj.x = x;
	obj.y = y;
	level.objects.push_back(std::move(obj));
}

void
place_tile_big(int tx, int ty, int t1, int t2)
{
	vec2_16 t1v = tiles->tile_index_to_xy(t1);
	vec2_16 t2v = tiles->tile_index_to_xy(t2);
	int chunk_width = (t2v.x - t1v.x) + 1;
	int chunk_height = (t2v.y - t1v.y) + 1;
	if (chunk_width < 1)
		chunk_width = 1;
	if (chunk_height < 1)
		chunk_height = 1;
	for (int y = ty; y < ty + chunk_height; ++y) {
		for (int x = tx; x < tx + chunk_width; ++x) {
			uint8_t i = tiles->tile_xy_to_index(t1v.x + (x - tx), t1v.y + (y - ty));
			uint16_t ax = x;
			uint16_t ay = y;
			// what is this math lol
			if (attrib & 0x01) // hflip
				ax = (tx * 2) - 1 + (chunk_width - x);
			if (attrib & 0x02) // vflip
				ay = (ty * 2) - 1 + (chunk_height - y);
			tiles->set_tile(ax, ay, i, attrib);
		}
	}
}

void
place_tile_block(int tx, int ty, tile_block_t &block)
{
	int i = 0;
	// TODO: checkbox to disable tile != 0 check
	for (int y = ty; y < ty + block.height; ++y) {
		for (int x = tx; x < tx + block.width; ++x) {
			if (i >= block.tiles.size() || i >= block.attribs.size() || block.tiles[i] == -1) {
				++i;
				continue;
			}
			uint8_t attr = (block.attribs[i] & 0x03) ^ attrib; // xor
			uint16_t ax = x;
			uint16_t ay = y;
			// what is this math lol
			if (attrib & 0x01) // hflip
				ax = (tx * 2) - 1 + (block.width - x);
			if (attrib & 0x02) // vflip
				ay = (ty * 2) - 1 + (block.height - y);
			tiles->set_tile(ax, ay, block.tiles[i], attr & 0x03);
			++i;
		}
	}
}

tile_block_t
block_from_map(int t1, int t2)
{
	vec2_16 t1v = tiles->map_index_to_xy(t1);
	vec2_16 t2v = tiles->map_index_to_xy(t2);
	if (t2v.x < t1v.x)
		std::swap(t2v.x, t1v.x);
	if (t2v.y < t1v.y)
		std::swap(t2v.y, t1v.y);
	tile_block_t block;
	block.width = (t2v.x - t1v.x);
	block.height = (t2v.y - t1v.y);

	for (int y = t1v.y; y < t2v.y; ++y) {
		for (int x = t1v.x; x < t2v.x; ++x) {
			block.tiles.push_back(tiles->get_tile(x, y));
			block.attribs.push_back(tiles->get_attrib(x, y));
			printc("Tile %d @ %d,%d", tiles->get_tile(x, y), x, y);
		}
	}

	return block;
}

/*
	TODO: FIXME:
		OUT OF BOUNDS PLACEMENT (BLOCK)


.zip level zlib
.gfx.lua resource loading table, texture and hitbox viewr, adjust offests and string ids,
add [+] button texture to object with file open
same with animations/sequences
quick add output window and R E A D Y 

texture viewer loader check source line/object (object texture tree view?)

	1v1 ghost (same level) 8 boss race elo
	buster offset edit
	serialize
	UNDOS!!!!!!
*/

void
update(void)
{
	tile_mousex = scroll_mousex / 16;
	tile_mousey = scroll_mousey / 16;
	if (!mouse_in_canvas())
		return;

	if (mode == MODE_TILE && !input::getkey(SDL_SCANCODE_LCTRL)) {
		if (input::getmousebutton(0)) {
			if (selected_level_tile_max == -1) {
				if (selected_tile_max > selected_tile)
					place_tile_big(tile_mousex, tile_mousey, selected_tile, selected_tile_max);
				else
					tiles->set_tile(tile_mousex, tile_mousey, selected_tile, attrib);
			} else if (!selecting_block) {
				place_tile_block(tile_mousex, tile_mousey, block_clipboard);
			}
		}
		if (input::getmousebutton(1)) {
			tiles->erase_tile(tile_mousex, tile_mousey);
		}
	} else if (mode == MODE_OBJECT) {
	}
}

void
draw(void)
{
	draw_grid();

	tiles->reset();
	tiles->draw(0, 0);
	if (mode == MODE_TILE) {
		if (selecting_block) {
			vec2_16 pos  = tiles->map_index_to_xy(selected_level_tile);
			vec2_16 size = tiles->map_index_to_xy(selected_level_tile_max);
			if (pos.x > size.x)
				std::swap(pos.x, size.x);
			if (pos.y > size.y)
				std::swap(pos.y, size.y);
			size.x -= pos.x;
			size.y -= pos.y;
			video::set_color(1.f, .9f, 0.f);
			video::draw_rect_line((pos.x*16)-1, (pos.y*16)-1, (size.x*16)+2, (size.y*16)+2);
			video::set_color(1.f, 1.f, 1.f, 1.f);
		}
		if (mouse_in_canvas() && !input::getmousebutton(1)) {
			if (selected_level_tile_max == -1) {
				//if (selected_tile_max > selected_tile)
				//	place_tile_big(tile_mousex, tile_mousey, selected_tile, selected_tile_max);
				//else
					tiles->draw_tile(tile_mousex * 16, tile_mousey * 16, selected_tile);
			} else if (!selecting_block) {
				tiles->draw_block(tile_mousex * 16, tile_mousey * 16, block_clipboard);
			}
		}
	}

	draw_objects();
	if (mode == MODE_OBJECT) {
		/*drag select tile block
		preview tile and object before placing
		hold down button*/
	}

	video::flush();
}

void
cleanup(void)
{
}

} // namespace
