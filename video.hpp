#ifndef XPP_VIDEO_HPP
#define XPP_VIDEO_HPP

#include "core.hpp"
#include <SDL2/SDL.h>
#include "bgfx/bgfx.h"
#include "bgfx/platform.h"
#include "renderer.hpp"

namespace xpp::video {
	extern SDL_Window *window;

	void	init(bgfx::RendererType::Enum renderer_type = bgfx::RendererType::OpenGL);
	int		width(void);
	int		height(void);
	void	reset_resize(int width, int height);
	void	cleanup(void);
}

#endif
