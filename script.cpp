#include "script.hpp"
#include "emulator.hpp"
#include "input.hpp"
#include "sound.hpp"
#include "animation.hpp"
#include "console.hpp"
#include "event.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include "xgui/TextEditor.h"

#include "renderer.hpp"
#include "tilemap.hpp"

sol::state lua;

namespace xpp::script {

namespace {

void
sort_scripts(void)
{
	/*std::sort(scripts.begin(), scripts.end(), [](const script_t &a, const script_t &b) -> bool { 
		return a.filename > b.filename; 
	});*/
}

} // namespace

// interface

std::unordered_map<std::string, script_t> scripts;

void
init(void)
{
	lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::coroutine,
		sol::lib::string, sol::lib::os, sol::lib::math, sol::lib::table, sol::lib::debug,
		sol::lib::io, sol::lib::bit32, sol::lib::ffi, sol::lib::jit);

	lua.script("require('ffi').cdef[[void printc(const char *fmt, ...);]] printc=ffi.C.printc");
	lua.script("ffi.cdef[[void printx(unsigned char level, const char *fmt, ...);]] printx=ffi.C.printx");
	lua["CONSOLE_WARN"] = CONSOLE_WARN;
	lua["CONSOLE_ERROR"] = CONSOLE_ERROR;
	lua["CONSOLE_LINK"] = CONSOLE_LINK;
	lua["io"]["bind"] = sol::overload(
		static_cast<bool(*)(int, std::string)>(&input::bind),
		static_cast<bool(*)(std::string, std::string)>(&input::bind)
	);
	lua["io"]["get_bind"] = sol::overload(
		static_cast<std::string(*)(int)>(&input::get_bind),
		static_cast<std::string(*)(std::string)>(&input::get_bind)
	);
	lua["io"]["unbind"] = sol::overload(
		static_cast<bool(*)(int)>(&input::unbind),
		static_cast<bool(*)(std::string)>(&input::unbind)
	);
	lua["io"]["unbindall"] = input::unbindall;
	lua["io"]["getkey"] = input::getkey;
	lua["io"]["getjoy"] = input::getjoy;
	lua["JOY_UP"] = RETRO_DEVICE_ID_JOYPAD_UP;
	lua["JOY_DOWN"] = RETRO_DEVICE_ID_JOYPAD_DOWN;
	lua["JOY_LEFT"] = RETRO_DEVICE_ID_JOYPAD_LEFT;
	lua["JOY_RIGHT"] = RETRO_DEVICE_ID_JOYPAD_RIGHT;
	lua["JOY_A"] = RETRO_DEVICE_ID_JOYPAD_A;
	lua["JOY_B"] = RETRO_DEVICE_ID_JOYPAD_B;
	lua["JOY_X"] = RETRO_DEVICE_ID_JOYPAD_X;
	lua["JOY_Y"] = RETRO_DEVICE_ID_JOYPAD_Y;
	lua["JOY_L"] = RETRO_DEVICE_ID_JOYPAD_L;
	lua["JOY_R"] = RETRO_DEVICE_ID_JOYPAD_R;
	lua["JOY_START"] = RETRO_DEVICE_ID_JOYPAD_START;
	lua["JOY_SELECT"] = RETRO_DEVICE_ID_JOYPAD_SELECT;

	lua["run_frames"] = 0;
	lua["game_running"] = false;
	lua["quit"] = exit;
	lua["event"] = sol::new_table();
	lua["event"]["intercept"] = sol::new_table();
	lua["event"]["journal"] = sol::new_table();
	lua["event"]["list"] = sol::new_table();
	lua["event"]["_fire_args"] = sol::overload(
		event::fire_lua,
		event::fire<std::string>,
		event::fire<int>
	);
	lua["video"] = sol::new_table();
	lua["video"]["set_color"] = video::set_color;
	lua["video"]["load_texture"] = [](std::string f, std::string id) -> video::texture {
		return video::load_texture("res/gfx/" + f + ".png", id);
	};
	lua["video"]["reload_texture"] = [](std::string id) -> video::texture {
		return video::reload_texture(id);
	};
	lua["video"]["draw_sprite"] = sol::overload(
		static_cast<void(*)(std::string, float, float, uint8_t)>(&video::draw_sprite),
		static_cast<void(*)(video::texture &, float, float, uint8_t)>(&video::draw_sprite),
		[](video::texture &t, float x, float y) { video::draw_sprite(t, x, y, 0); }
	);
	lua["video"]["set_blend"] = sol::overload(
		video::reset_blend,
		video::set_blend
	);
	lua["video"]["set_mask"] = video::set_mask;
	lua["video"]["draw_sprite_masked"] = video::draw_sprite_masked;
	lua["video"]["draw_line"] = video::draw_line;
	lua["video"]["draw_rectline"] = video::draw_rect_line;
	lua["video"]["draw_rect"] = video::draw_rect;
	lua["video"]["flush_primitives"] = video::flush_primitives;
	lua["video"]["set_palette"] = sol::overload(
		static_cast<void(*)(uint8_t)>(&video::set_palette),
		static_cast<void(*)(void)>(&video::set_palette)
	);
	lua["video"]["push_palette"] = video::push_palette;
	//moth_atlas:sub("intro"):draw(32, 32);

	sol::usertype<video::xfont> font_type = lua.new_usertype<video::xfont>("font",
		sol::constructors<
			video::xfont(std::string),
			video::xfont(std::string, char, char)
		>()
	);
	font_type["draw_char"] = &video::xfont::draw_char;
	font_type["draw_text"] = &video::xfont::draw_text;
	font_type["set_char_size"] = &video::xfont::set_char_size;

	lua["sound"] = sol::new_table();
	lua["sound"]["new"] = [](std::string f, std::string id) -> bool {
		if (sound::load_sound("res/snd/" + f, id) == NULL)
			return true;
		else
			return false;
	};
	lua["sound"]["play"] = sol::overload(
		static_cast<cs_playing_sound_t *(*)(cs_loaded_sound_t *)>(&sound::play_sound),
		static_cast<cs_playing_sound_t *(*)(std::string)>(&sound::play_sound)
	);
	lua["sound"]["stop"] = sound::stop_sound;
	/*lua["animation"] = sol::new_table();
	lua["animation"]["load"] = [](std::string id) -> animation_t {
		animation_t anim;
		anim.load("res/seq/" + id + ".seq");
		return anim;
	};*/

	sol::usertype<animation_t> animation_type = lua.new_usertype<animation_t>("animation",
		sol::constructors<animation_t(), animation_t(std::string)>()
	);
	animation_type["load"] = &animation_t::load;
	animation_type["reset"] = sol::overload(
		static_cast<void(animation_t::*)(uint8_t)>(&animation_t::reset),
		static_cast<void(animation_t::*)(void)>(&animation_t::reset)
	);
	animation_type["play"] = sol::overload(
		static_cast<void(animation_t::*)(uint8_t)>(&animation_t::play),
		static_cast<void(animation_t::*)(void)>(&animation_t::play)
	);
	animation_type["pause"] = sol::overload(
		static_cast<void(animation_t::*)(uint8_t)>(&animation_t::pause),
		static_cast<void(animation_t::*)(void)>(&animation_t::pause)
	);
	animation_type["advance"] = sol::overload(
		static_cast<bool(animation_t::*)(uint8_t)>(&animation_t::advance),
		static_cast<bool(animation_t::*)(void)>(&animation_t::advance)
	);
	animation_type["update"] = sol::overload(
		static_cast<bool(animation_t::*)(uint8_t)>(&animation_t::update),
		static_cast<bool(animation_t::*)(void)>(&animation_t::update)
	);
	animation_type["length"] = sol::overload(
		static_cast<uint8_t(animation_t::*)(uint8_t)>(&animation_t::length),
		static_cast<uint8_t(animation_t::*)(void)>(&animation_t::length)
	);
	animation_type["frame"] = sol::overload(
		static_cast<uint8_t(animation_t::*)(uint8_t)>(&animation_t::frame),
		static_cast<uint8_t(animation_t::*)(void)>(&animation_t::frame)
	);
	animation_type["finished"] = sol::overload(
		static_cast<bool(animation_t::*)(uint8_t)>(&animation_t::finished),
		static_cast<bool(animation_t::*)(void)>(&animation_t::finished)
	);
	animation_type["draw"] = &animation_t::draw;
	animation_type["draw_layer"] = &animation_t::draw_layer;

	lua["sfc"] = sol::new_table();

	/*sol::overload(
		static_cast<cs_loaded_sound_t *(*)(std::string)>(&sound::load_sound),
		static_cast<cs_loaded_sound_t *(*)(std::string, std::string)>(&sound::load_sound)
	);*/
	/*lua["video"]["load_texture"] = sol::overload(
		static_cast<video::texture(*)(std::string)>(&video::load_texture),
		static_cast<video::texture(*)(std::string, std::string)>(&video::load_texture)
	);*/
}

void
load_kernel(void)
{
	puts("KERNEL SETUP!");
	printc("KERNEL SETUP!");
	std::stringstream ss;
	std::ifstream f("kernel/kernel.lua");
	ss << f.rdbuf();
	f.close();
	run(ss.str(), "kernel.lua");
	for (const auto &i : std::filesystem::directory_iterator("kernel")) {
		if (i.path().extension() != ".lua" || i.path().filename() == "kernel.lua")
			continue;

		std::stringstream ss;
		std::ifstream f(i.path());
		ss << f.rdbuf();
		f.close();

		run(ss.str(), i.path().filename());
	}
}

bool
load(std::string filename, bool exec)
{
	script_t script;
	std::string key = std::filesystem::path(filename);
	if (scripts.find(key) == scripts.end()) {
		std::stringstream ss;
		std::filesystem::path path(filename);

		script.editor = new TextEditor();
		script.editor->SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
		script.editor->SetPalette(TextEditor::GetLightPalette());

		script.path = path;
		script.filename = path.filename();

		std::ifstream f(path);
		ss << f.rdbuf();
		script.editor->SetText(ss.str());
		script.text = script.editor->GetText();
		f.close(); // important for rename/remove to work(?)
		script.modified = false;

		scripts[script.path] = script;
	}
	if (exec)
		run(scripts[key].text, scripts[key].filename);

	sort_scripts();
	return true;
}

bool
load_dir(std::string dir, bool exec)
{
	std::string key;
	for (const auto &i : std::filesystem::directory_iterator(dir)) {
		key = i.path();
		if (scripts.find(key) == scripts.end()) {
			script_t script;
			std::stringstream ss;

			if (i.path().extension() != ".lua") {
				printc("Not loading %s\n", i.path().c_str());
				continue;
			}

			script.editor = new TextEditor();
			script.editor->SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
			script.editor->SetPalette(TextEditor::GetLightPalette());

			script.path = i.path();
			script.filename = i.path().filename();

			std::ifstream f(i.path());
			ss << f.rdbuf();
			script.editor->SetText(ss.str());
			script.text = script.editor->GetText();
			f.close(); // important for rename/remove to work
			script.modified = false;

			scripts[script.path] = script;
			if (exec)
				run(scripts[key].text, scripts[key].filename);
		}
	}

	sort_scripts();
	return true;
}

// TODO: what about file line numbers?
bool
run(std::string str, std::string filename)
{
	try {
		lua.safe_script(str);
		return true;
	} catch (const sol::error &e) {
		printx(CONSOLE_ERROR, "%s", e.what());
		if (filename != "" && parse_sol_line_number_chunk(e.what()) > 0) {
			size_t i = printx(CONSOLE_LINK, "%s", filename.c_str());
			console::lines[i].number = parse_sol_line_number_chunk(e.what());
		}
		return false;
	}
}

bool
save(std::string s, std::string filename)
{
	if (filename == "")
		filename = scripts[s].path;

	// atomic
	std::ofstream f((filename + ".tmp").c_str());
	// fix newline build-up
	f << scripts[s].editor->GetText().substr(0, scripts[s].editor->GetText().size()-1);
	f.close();
	if (rename((filename + ".tmp").c_str(), filename.c_str()) != 0) {
		printc("Failed to save/rename script %s: %s", filename.c_str(), strerror(errno));
		printf("Failed to save/rename script %s: %s\n", filename.c_str(), strerror(errno));
		return false;
	}

	// clean up in case
	if (remove((filename + ".tmp").c_str()) != 0) {}

	scripts[s].text = scripts[s].editor->GetText();
	scripts[s].modified = false;

	return true;
}

bool
check_result(sol::protected_function_result r, std::string filename)
{
	if (r.valid())
		return true;

	sol::error e = r;
	printx(CONSOLE_ERROR, "%s", e.what());
	if (filename != "" && parse_sol_line_number_chunk(e.what()) > 0) {
		size_t i = printx(CONSOLE_LINK, "%s", filename.c_str());
		console::lines[i].number = parse_sol_line_number_chunk(e.what());
	}

	return false;
}

std::string
to_string(sol::object o)
{
	// BROKEN FIXME TODO
	//if (((int)(o.get_type())) == LUA_TFUNCTION) {
	//	sol::object os = lua["debug"]["function_signature"](o);
	//	if (os.is<std::string>())
	//		return os.as<std::string>();
	//	else
	//		return "nil()";
	//} else {
		sol::object os = lua["tostring"](o);
		return os.as<std::string>();
		//return ((sol::object)(lua["tostring"](o))).as<std::string>();
	//}
}

std::string
type_to_string(int t)
{
	//sol::object = lua["type"](pop?);
	switch (t) {
		case LUA_TNONE:			return "none";
		case LUA_TNIL:			return "nil";
		case LUA_TTABLE:			return "table"; // usually last
		case LUA_TSTRING:			return "string";
		case LUA_TNUMBER:		return "number";
		case LUA_TTHREAD:			return "thread";
		case LUA_TBOOLEAN:		return "bool";
		case LUA_TFUNCTION:		return "function";
		case LUA_TUSERDATA:		return "userdata";
		case LUA_TLIGHTUSERDATA:	return "pointer";
		case 10:					return "ffi"; // ???
		default:					return "unknown";
	}
}

std::string
type_to_string(sol::type t)
{
	return type_to_string((int)t);
}

// NOTE: capture id's start at 1 not 0, 0 is full string

// unused
std::string
parse_sol_filename(std::string str)
{
	//std::regex r(": (\\w+\\.lua):(\\d+): ");
	std::regex r("(\\w+\\.lua):(\\d+)");
	std::smatch m;
	std::regex_search(str, m, r);

	if (m.size() >= 3)
		return m.str(1);

	return "nil.lua";
}

int
parse_sol_line_number(std::string str)
{
	//std::regex r(": (\\w+\\.lua):(\\d+): ");
	std::regex r("(\\w+\\.lua):(\\d+)");
	std::smatch m;
	std::regex_search(str, m, r);

	if (m.size() >= 3)
		return std::stoi(m.str(2));

	return 0;
}

int
parse_sol_line_number_chunk(std::string str)
{
	std::regex r("]:(\\d+): ");
	std::smatch m;
	std::regex_search(str, m, r);

	if (m.size() >= 2)
		return std::stoi(m.str(1));

	return 0;
}

} // namespace
