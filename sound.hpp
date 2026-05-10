#ifndef XPP_SOUND_HPP
#define XPP_SOUND_HPP

#include "core.hpp"
#include "cute_sound.h"

namespace xpp::sound {
	void init(void);
	void mix(void);
	cs_loaded_sound_t *load_sound(std::string filename);
	cs_loaded_sound_t *load_sound(std::string filename, std::string id);
	cs_playing_sound_t *play_sound(cs_loaded_sound_t *sound);
	cs_playing_sound_t *play_sound(std::string id);
	void stop_sound(cs_playing_sound_t *playing);
};

#endif
