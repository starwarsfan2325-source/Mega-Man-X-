#include "video.hpp"
#include <SDL2/SDL_syswm.h>
#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION

#define WPOS (SDL_WINDOWPOS_CENTERED)
#define WFLAGS (SDL_WINDOW_SHOWN)

namespace xpp::video {

namespace {

int swaps = 0;
int w_width = 1800;
int w_height = 1350;
//int w_width = 256 * 4;
//int w_height = 224 * 4;

void
init_gfx(bgfx::RendererType::Enum renderer_type)
{
	SDL_SysWMinfo wmi;
	SDL_VERSION(&wmi.version);
	if (!SDL_GetWindowWMInfo(window, &wmi))
		panic("Failed to get window info");

	bgfx::PlatformData pdata;
	pdata.ndt = wmi.info.x11.display;
	pdata.nwh = (void *)(uintptr_t)wmi.info.x11.window;
	pdata.context = NULL;
	bgfx::setPlatformData(pdata);

	bgfx::Init init;
	init.type = renderer_type;
	init.vendorId = BGFX_PCI_ID_NONE;
	init.resolution.width = w_width;
	init.resolution.height = w_height;
	//init.resolution.reset = BGFX_RESET_VSYNC;
	bgfx::init(init);
	bgfx::setDebug(BGFX_DEBUG_TEXT);
}

void
create_window(int width, int height, bgfx::RendererType::Enum renderer_type)
{
	atexit(SDL_Quit);
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
		panic("SDL_INIT_VIDEO failed: %s\n", SDL_GetError());

	window = SDL_CreateWindow("X++", WPOS, WPOS, width, height, WFLAGS);
	if (window == NULL)
		panic("SDL_CreateWindow failed: %s\n", SDL_GetError());

	init_gfx(renderer_type);
}

} // namespace

// interface

SDL_Window *window = NULL;

void
init(bgfx::RendererType::Enum renderer_type)
{
	create_window(w_width, w_height, renderer_type);
	//bgfx::setViewClear(255, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x443355FF, 1.0f, 0);
	//bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x443355FF, 1.0f, 0);
	bgfx::setViewClear(255, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x443355FF, 1.0f, 0);
	bgfx::setViewRect(255, 0, 0, w_width, w_height);
	bgfx::reset(w_width, w_height, BGFX_RESET_VSYNC /* _NONE disable vsync */);
}

int
width(void)
{
	return w_width;
}

int
height(void)
{
	return w_height;
}

// NOTE: resizing too fast manually causes vulkan device lost error
void
reset_resize(int width, int height)
{
	w_width = width;
	w_height = height;
	bgfx::reset(width, height, BGFX_RESET_NONE);
	bgfx::setViewRect(0, 0, 0, width, height);
}

void
cleanup(void)
{
	bgfx::shutdown();
}

} // namespace
