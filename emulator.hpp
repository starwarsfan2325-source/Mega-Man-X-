#ifndef XPP_EMULATOR_HPP
#define XPP_EMULATOR_HPP

#include "bsnes.hpp"

#include "libretro.h"
#include "video.hpp"
//#include <GL/glew.h>
//#include <GLFW/glfw3.h>
#include <alsa/asoundlib.h>
#include <string>

namespace xpp::emulator {
	struct retro_keymap {
		unsigned k;
		unsigned rk;
	};

	extern retro_keymap g_binds[12];

	extern int16_t sndbuf[2];
	extern int16_t *audio_log;
	extern int16_t audio_counter;

	extern bool focused;
	extern bool running;
	extern bool mute;
	extern int run_frames;
	extern std::string rom;

	void		init(const char *core);
	void		load_rom(const char *filename);
	void		set_window(SDL_Window *window);
	void		set_vertices(float x1, float y1, float x2, float y2);
	void		vertices_auto(int x, int y, int w = 256, int h = 224);
	void		pause(void);
	void		resume(void);
	void		set_running(bool r);
	void		run(void);
	bgfx::TextureHandle	get_texture(void);
	void		draw(int x, int y, int w = 256, int h = 224);
	void		draw(void);
	void		*get_ram(void);
	size_t		get_ram_size(void);
	void		*get_vram(void);
	size_t		get_vram_size(void);
	void		*get_palette(void);
	size_t		get_palette_size(void);
	bool		save_state(void *data, size_t size);
	bool		load_state(const void *data, size_t size);
	size_t		get_state_size(void);
	uint8_t		read_u8(uint16_t address);
	int8_t		read_s8(uint16_t address);
	uint16_t	read_u16_le(uint16_t address);
	void		write_u8(uint16_t address, uint8_t value);
	void		cleanup(void);
}

// SNES specific interface
namespace vblank::emulator::snes {
}

#endif