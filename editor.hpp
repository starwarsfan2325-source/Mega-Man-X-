#ifndef XPP_EDITOR_HPP
#define XPP_EDITOR_HPP

#include "core.hpp"
#include "video.hpp"
#include "tilemap.hpp"
#include <unordered_map>

namespace xpp::editor {
	enum {
		MODE_TILE,
		MODE_OBJECT,
		MODE_EVENT,
	};
	struct object_property_t {
		char input[48] = {'\0'};
		float number = 0.0;
		bool boolean = false;
		int combo_id = 0;
		const char* combo = "Combo\0";
		std::string type = "number";
		std::string name = "<nil>";

		object_property_t(std::string _name = "<nil>", std::string _type = "number")
		{ name = _name; type = _type; }
	};

	struct object_entry_t {
		video::texture preview;
		video::texture level_preview;
		std::string name = "<nil>";
		std::string id = "empty";
		std::vector<object_property_t> properties;

		object_property_t &add_property_combo(std::string name,
			const char *combo, std::string _default = "<nil>")
		{
			object_property_t p(name, "combo");
			p.combo = combo;
			if (_default == "<nil>")
				strncpy(p.input, combo, 47);
			else
				strncpy(p.input, _default.c_str(), 47);
			properties.push_back(std::move(p));
			return properties[properties.size()];
		}
		object_property_t &add_property_bool(std::string name, bool _default = false)
		{
			object_property_t p(name, "boolean");
			p.boolean = _default;
			properties.push_back(std::move(p));
			return properties[properties.size()];
		}
		object_property_t &add_property_string(std::string name, std::string _default = "")
		{
			object_property_t p(name, "string");
			strncpy(p.input, _default.c_str(), 47);
			properties.push_back(std::move(p));
			return properties[properties.size()];
		}
		object_property_t &add_property_number(std::string name, float _default = 0.f)
		{
			object_property_t p(name, "number");
			p.number = _default;
			properties.push_back(std::move(p));
			return properties[properties.size()];
		}
	};

	struct level_object_t {
		std::vector<object_property_t> properties;
		std::string id = "empty";
		int x = 0, y = 0;
		bool flip = false;
		bool respawn = true;
	};

	struct level_t {
		std::vector<level_object_t> objects;
	};

	extern level_t level;
	extern std::unordered_map<std::string, object_entry_t> object_list;
	extern std::string selected_object_entry;
	extern int selected_object;
	extern int selected_tile;
	extern int selected_tile_max;
	extern int selected_level_tile;
	extern int selected_level_tile_max;
	// updated by editor window
	extern int mousex;
	extern int mousey;
	extern int scroll_mousex;
	extern int scroll_mousey;
	extern int scrollx;
	extern int scrolly;
	extern int canvas_window_width;
	extern int canvas_window_height;
	extern int mode;
	extern uint8_t attrib;
	extern bool selecting_block;
	extern tile_block_t block_clipboard;
	extern std::vector<tile_block_t> stamps;
	extern video::tilemap *tiles;

	void init(void);
	bool mouse_in_canvas(void);
	object_entry_t &add_object_entry(std::string id, std::string name, video::texture preview);
	// TODO: use texture id instead of file?
	object_entry_t &add_object_entry(std::string id, std::string name, std::string preview_file);
	void place_object(std::string id, int x, int y);
	tile_block_t block_from_map(int t1, int t2);
	void update(void);
	void draw(void);
	void cleanup(void);
}

#endif
