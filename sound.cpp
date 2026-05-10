#include "sound.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unordered_map>
#include <filesystem>

#include "SDL2/SDL.h"
#define CUTE_SOUND_IMPLEMENTATION
#define CUTE_SOUND_FORCE_SDL
#define XPP_CUTE_SOUND_USE_POP_FIX
#include "cute_sound.h"

namespace xpp::sound {

namespace {

std::unordered_map<std::string, cs_loaded_sound_t *> sounds;
std::unordered_map<std::string, cs_loaded_sound_t *> sound_files;
//std::vector<cs_playing_sound_t> playing;
cs_context_t *context;

} //namespace

// interface

// TODO: mix only on update for emulator-like sound behavior

void
init(void)
{
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
		panic("SDL_INIT_AUDIO failed: %s\n", SDL_GetError());

	// TODO: make cvar or launch option for 3rd argument (buffer size)
	//context = cs_make_context(NULL, 441000, 1470, 32, NULL);
	//context = cs_make_context(NULL, 44100, 1024, 32, NULL);
	// SHOULD BE 1024 FOR GOOD PC
	//context = cs_make_context(NULL, 44100, 2048, 32, NULL);
	context = cs_make_context(NULL, 44100, 4096, 32, NULL);
	if (context == NULL)
		panic("Failed to initialize cute_sound: %s", cs_error_reason);

	//cs_spawn_mix_thread(context);
}

// was using cs_spawn_mix_thread(context), however i suspect
// it was the reason i kept getting segfaults in libasound,
// maybe some kind of race condition
void
mix(void)
{
	cs_mix(context);
}

cs_loaded_sound_t *
load_sound(std::string filename)
{
	cs_loaded_sound_t *sound_ptr = NULL; // touch it for return value of load_sound(id)
	cs_loaded_sound_t sound;

	if (!std::filesystem::exists(filename)) {
		printx(CONSOLE_ERROR, "Failed to load sound: %s (not found)", filename.c_str());
		return sound_ptr;
	}

	sound = cs_load_wav(filename.c_str());

	if (sound.sample_rate == 0) {
		printx(CONSOLE_ERROR, "Failed to load sound: %s (%s)", filename.c_str(), cs_error_reason);
		return sound_ptr;
	}

	sound_ptr = (cs_loaded_sound_t *)malloc(sizeof(cs_loaded_sound_t));
	*sound_ptr = std::move(sound);
	sound_files[filename] = sound_ptr;

	return sound_ptr;
}

cs_loaded_sound_t *
load_sound(std::string filename, std::string id)
{
	if (sound_files.find(filename) != sound_files.end())
		sounds[id] = sound_files[filename];
	else
		sounds[id] = load_sound(filename);

	return sounds[id];
}

cs_playing_sound_t *
play_sound(cs_loaded_sound_t *sound)
{
	if (sound == NULL) {
		printx(CONSOLE_WARN, "Tried to play NULL sound.");
		return NULL;
	}
	cs_play_sound_def_t def = cs_make_def(sound);
	cs_playing_sound_t *playing = cs_play_sound(context, def);
	return playing;
}

cs_playing_sound_t *
play_sound(std::string id)
{
	if (sounds.find(id) != sounds.end()) {
		return play_sound(sounds[id]);
	} else {
		printx(CONSOLE_WARN, "Tried to play invalid sound ID: %s", id.c_str());
		return NULL;
	}
}

void
stop_sound(cs_playing_sound_t *playing)
{
	if (playing == NULL)
		return;

	cs_stop_sound(playing);
}

/*void
stop_sound(std::string id)
{
	if (sounds.find(id) == sounds.end() || sounds[id] == NULL) {
		printx(CONSOLE_WARN, "Tried to stop invalid sound ID: %s", id.c_str());
		return;
	}
	cs_stop_sound(sounds[id]);
}*/

} // namespace
