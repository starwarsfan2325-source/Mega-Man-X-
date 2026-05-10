#include "console.hpp"
#include "event.hpp"
#include "video.hpp"
#include "emulator.hpp"
#include <map>

namespace xpp::input {

namespace {

std::map<int, std::string> binds;
static std::map<int, int> joy_state;
//static std::map<int, int> joy_state_last;
const Uint8	*kbd_state_sdl;
Uint8 *kbd_state_last;
Uint8 *kbd_state;
int kbd_nkeys;
int mouse_x = 0;
int mouse_y = 0;
Uint32 mouse_buttons = 0;

} // namespace

// interface

void
init(void)
{
	for (int i = 0; i < 16; ++i)
		joy_state[i] = 0;

	console::command::add_action("up", [](bool act) {
		joy_state[RETRO_DEVICE_ID_JOYPAD_UP] = act ? 1 : 0;});
	console::command::add_action("down", [](bool act) {
		joy_state[RETRO_DEVICE_ID_JOYPAD_DOWN] = act ? 1 : 0;});
	console::command::add_action("left", [](bool act) {
		joy_state[RETRO_DEVICE_ID_JOYPAD_LEFT] = act ? 1 : 0;});
	console::command::add_action("right", [](bool act) {
		joy_state[RETRO_DEVICE_ID_JOYPAD_RIGHT] = act ? 1 : 0;});
	console::command::add_action("a", [](bool act) {
		joy_state[RETRO_DEVICE_ID_JOYPAD_A] = act ? 1 : 0;});
	console::command::add_action("b", [](bool act) {
		joy_state[RETRO_DEVICE_ID_JOYPAD_B] = act ? 1 : 0;});
	console::command::add_action("x", [](bool act) {
		joy_state[RETRO_DEVICE_ID_JOYPAD_X] = act ? 1 : 0;});
	console::command::add_action("y", [](bool act) {
		joy_state[RETRO_DEVICE_ID_JOYPAD_Y] = act ? 1 : 0;});
	console::command::add_action("l", [](bool act) {
		joy_state[RETRO_DEVICE_ID_JOYPAD_L] = act ? 1 : 0;});
	console::command::add_action("r", [](bool act) {
		joy_state[RETRO_DEVICE_ID_JOYPAD_R] = act ? 1 : 0;});
	console::command::add_action("start", [](bool act) {
		joy_state[RETRO_DEVICE_ID_JOYPAD_START] = act ? 1 : 0;});
	console::command::add_action("select", [](bool act) {
		joy_state[RETRO_DEVICE_ID_JOYPAD_SELECT] = act ? 1 : 0;});
	console::command::add_action("neon", [](bool act) {
		joy_state[RETRO_DEVICE_ID_JOYPAD_Y] = act ? 1 : 0;
		joy_state[RETRO_DEVICE_ID_JOYPAD_B] = act ? 1 : 0;
	});

	kbd_state_sdl = SDL_GetKeyboardState(&kbd_nkeys);
	kbd_state_last = (Uint8 *)calloc(kbd_nkeys, 1);
	kbd_state = (Uint8 *)calloc(kbd_nkeys, 1);
	SDL_PumpEvents();
}

void
update(void)
{
	memcpy(kbd_state_last, kbd_state_sdl, kbd_nkeys);
	SDL_PumpEvents();

	for (int i = 0; i < kbd_nkeys; ++i) {
		kbd_state[i] = kbd_state_sdl[i];
		if (kbd_state[i] == 1 && kbd_state_last[i] == 0) {
			kbd_state[i] = 2; // pressed
			event::fire<int>("input.key_press", i);
			if (binds.find(i) != binds.end())
				console::send(binds[i]);
		} else if (kbd_state[i] == 0 && kbd_state_last[i] == 1) {
			kbd_state[i] = 3; // released
			event::fire<int>("input.key_release", i);
			if (binds.find(i) != binds.end() && binds[i].size() > 1 && binds[i][0] == '+')
				console::send(std::string("-").append(binds[i].substr(1)));
		}
	}

	int left_old = mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT);
	mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
	int left = mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT);
	if (left and !left_old)
		event::fire<int>("input.mouse_press", 0);
}

bool
bind(int key, std::string command)
{
	binds[key] = command;
	return true;
}

bool
bind(std::string key, std::string command)
{
	sol::object ikey = lua["string_to_key"][key];
	if (ikey.valid() && ikey.is<int>()) {
		binds[ikey.as<int>()] = command;
		return true;
	} else {
		printx(CONSOLE_WARN, "Unknown key: \"%s\"", key.c_str());
		return false;
	}
}

std::string
get_bind(int key)
{
	if (binds.find(key) != binds.end())
		return binds[key];
	else
		return "";
}

std::string
get_bind(std::string key)
{
	sol::object ikey = lua["string_to_key"][key];
	if (ikey.valid() && ikey.is<int>() && binds.find(ikey.as<int>()) != binds.end())
		return binds[ikey.as<int>()];
	else
		return "";
}

bool
unbind(int key)
{
	binds.erase(key);
	return true;
}

bool
unbind(std::string key)
{
	sol::object ikey = lua["string_to_key"][key];
	if (ikey.valid() && ikey.is<int>()) {
		binds.erase(ikey.as<int>());
		return true;
	} else {
		printx(CONSOLE_WARN, "Unknown key: \"%s\"", key.c_str());
		return false;
	}
}

void
unbindall(void)
{
	binds.clear();
}

int
getkey(int key)
{
	return kbd_state[key];
}

int
getmousex(void)
{
	return mouse_x;
}

int
getmousey(void)
{
	return mouse_y;
}

int
getmousebutton(int button)
{
	if (button == 0)
		return mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT);
	else if (button == 1)
		return mouse_buttons & SDL_BUTTON(SDL_BUTTON_RIGHT);
	else
		return 0;
}

bool
getjoy(int button)
{
	return (bool)joy_state[button];
}

/*bool
getjoypressed(int button)
{
	if (joy_state[button] != 0 && joy_state_last[button] == 0)
		return true;
	else
		return false;
}

bool
getjoyreleased(int button)
{
	if (joy_state[button] == 0 && joy_state_last[button] != 0)
		return true;
	else
		return false;
}*/

} // namespace
