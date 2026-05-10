#include "engine.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include <dlfcn.h>

#include "input.hpp"
#include "vblank.hpp"
#include "video.hpp"
#include "event.hpp"

extern "C" {
#include "engine/api.h"
#include "engine/ppu.h"
}

using namespace vblank;

namespace vblank::engine {

bool focused = false;
bool running = false;
int run_frames = 0;
std::string rom = "empty";
GLuint screen = 0;
unsigned *screen_buffer = NULL;

namespace {

uint8_t *wram;
uint8_t *vram;
GLFWwindow *g_win = NULL;

static void
video_init(void)
{
	if (!g_win)
		panic("No window passed to engine");

	glGenTextures(1, &screen);

	if (!screen)
		panic("Failed to create the screen texture");

	glBindTexture(GL_TEXTURE_2D, screen);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GL_PIXEL_FIX;
	// GL_UNSIGNED_SHORT_5_5_5_1
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 224, 0,
				 GL_RGBA, GL_UNSIGNED_BYTE, screen_buffer);
	glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace

// interface

void
init(void)
{
	vblank_init();
	screen_buffer = vblank_get_screen_buffer();
	set_window(video::window);
	video_init();
	running = true;
}

void
load_rom(const char *filename)
{
	vblank_load_rom(filename);

	rom = std::string(filename);

	event::fire<std::string>("vblank.load_rom", rom);

	return;

v_error:
	panic("Failed to load cartridge %s", filename);
}

void
set_window(GLFWwindow *window)
{
	g_win = window;
}

void
pause(void)
{
	if (running) {
		running = false;
		event::fire("vblank.pause");
	}
}

void
resume(void)
{
	if (!running) {
		run_frames = 0;
		running = true;
		event::fire("vblank.resume");
	}
}

void
set_running(bool r)
{
	if (running && !r) {
		event::fire("vblank.pause");
	} else if (!running && r) {
		run_frames = 0;
		event::fire("vblank.resume");
	}

	running = r;
}

void
run(void)
{
	vblank_run();

	glBindTexture(GL_TEXTURE_2D, screen);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 224,
		GL_RGBA, GL_UNSIGNED_BYTE, screen_buffer);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void *
get_vram(void)
{
	return ppu_get_vram();
}

size_t
get_vram_size(void)
{
	return 0x20000;
}

void *
get_palette(void)
{
	return NULL;
}

size_t
get_palette_size(void)
{
	return 0;
}

void
cleanup(void)
{
}

} // namespace engine
