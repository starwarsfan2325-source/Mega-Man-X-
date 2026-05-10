#ifndef VBLANK_ENGINE_HPP
#define VBLANK_ENGINE_HPP

#include "emulator.hpp"

namespace vblank::engine {
	extern bool focused;
	extern bool running;
	extern int run_frames;
	extern std::string rom;
	extern GLuint screen;

	void		init(void);
	void		load_rom(const char *filename);
	void		set_window(GLFWwindow *window);
	void		pause(void);
	void		resume(void);
	void		set_running(bool r);
	void		run(void);
	void		*get_vram(void);
	size_t		get_vram_size(void);
	void		*get_palette(void);
	size_t		get_palette_size(void);
	bool		save_state(void *data, size_t size);
	bool		load_state(const void *data, size_t size);
	size_t		get_state_size(void);
	// TODO: vram as lua array using metatable
	uint8_t		read_u8_vram(uint16_t address);
	void		write_u8_vram(uint16_t address, uint8_t value);
	void		cleanup(void);
}

#endif