#ifndef XPP_RENDERER_HPP
#define XPP_RENDERER_HPP

#include "core.hpp"
#include <SDL2/SDL.h>
#include "bgfx/bgfx.h"
#include <unordered_map>
#include "sol/sol.hpp"

#define UV_VERSION 1

//#define ALPHA_STATE BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,BGFX_STATE_BLEND_INV_SRC_ALPHA)
#define ALPHA_STATE (BGFX_STATE_WRITE_RGB \
		| BGFX_STATE_WRITE_A \
		/*| BGFX_STATE_WRITE_Z \*/\
		| BGFX_STATE_BLEND_ALPHA \
		| BGFX_STATE_DEPTH_TEST_GREATER \
		| BGFX_STATE_CULL_CW)

#define ADD_STATE \
		(BGFX_STATE_WRITE_RGB \
		| BGFX_STATE_WRITE_A \
		/*| BGFX_STATE_WRITE_Z \*/\
		| BGFX_STATE_BLEND_ADD \
		/*| BGFX_STATE_DEPTH_TEST_GREATER \*/\
		| BGFX_STATE_CULL_CW)

#define INVERT_STATE (BGFX_STATE_WRITE_RGB \
		| BGFX_STATE_WRITE_A \
		| BGFX_STATE_BLEND_INV_DST_COLOR \
		| BGFX_STATE_CULL_CW)

// TODO: return empty texture when load error so no segfault
// and ref counting for destroy

// imvec2 TODO replace me with declaration
#include "../examples/common/imgui/imgui.h"

struct vec2_16 {
	uint16_t x;
	uint16_t y;
};

struct tile_block_t {
	std::vector<uint8_t> tiles;
	std::vector<uint8_t> attribs;
	int width = 1;
	int height = 1;
};

namespace xpp::video {
	extern uint16_t view_id;
	extern uint64_t blend_mode;

	struct vertex2 {
		float x, y;
	};
	struct vertex3 {
		float x, y, z;
	};
	struct vertex4 {
		float x, y, z, w;
	};
	struct color_t {
		float r, g, b, a;
	};

	struct sprite_vertex {
		vertex3 pos;
		vertex2 uv;
		float	palette;
		color_t color;
	} __attribute__((packed));
	struct primitive_vertex {
		vertex3 pos;
		color_t color;
	} __attribute__((packed));

	struct subtexture {
		vertex2 uv0 = {0.f, 0.f};
		vertex2 uv1 = {1.f, 1.f};
	};

	struct texture {
		std::string filename = "";
		float width = 0.f;
		float height = 0.f;
		vertex2 uv0 = {0.f, 0.f};
		vertex2 uv1 = {1.f, 1.f};
		bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle handle_idx = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle handle_palette = BGFX_INVALID_HANDLE;
		uint8_t palette_size = 0;
		// TODO: store as pointer. copying this around is *EXTREMELY* BAD
		std::unordered_map<std::string, subtexture> subtextures;

		// TODO: ensure never returns negative / underflows (abs)
		uint16_t
		subwidth(std::string subid = "")
		{
			if (subid.size() == 0)
				return (uint16_t)width;
			else
				return (uint16_t)(width*(subtextures[subid].uv1.x-subtextures[subid].uv0.x));
		}
		uint16_t
		subheight(std::string subid = "")
		{
			if (subid.size() == 0)
				return (uint16_t)height;
			else
				return (uint16_t)(height*(subtextures[subid].uv1.y-subtextures[subid].uv0.y));
		}
	};

	struct shader {
		bgfx::ShaderHandle fragment;
		bgfx::ShaderHandle vertex;
		bgfx::ProgramHandle program;
	};

	struct xfont {
		texture atlas;
		vertex2 char_fsize = {0.f, 0.f};
		uint8_t char_psize_x = 8;
		uint8_t char_psize_y = 8;
		char char_min = 'a';
		char char_max = 'z';

		xfont(std::string filename);
		xfont(std::string filename, char min, char max);
		void draw_char(char ch, float x, float y);
		void draw_text(std::string str, float x, float y);
		void
		set_char_size(uint8_t w, uint8_t h)
		{
			char_psize_x = w;
			char_psize_y = h;
			char_fsize.x = (float)w / atlas.width;
			char_fsize.y = (float)h / atlas.height;
		}
	};

	// inspect like oam?

	struct draw_cmd {
		// should probably make this an rgba int?
		color_t color = {1.f, 1.f, 1.f, 1.f};
		float depth = 0.f;
	};

	struct rect_cmd : draw_cmd {
		float x = 0.f;
		float y = 0.f;
		float w = 0.f;
		float h = 0.f;
	};

	struct line_cmd : draw_cmd {
		float x1 = 0.f;
		float y1 = 0.f;
		float x2 = 0.f;
		float y2 = 0.f;
	};

	// would store pos as uint16_t to save space, but int->float is expensive
	// TODO: pre-calculate uv0/uv1 flips
	struct sprite_cmd : draw_cmd {
		std::string id = "void";
		vertex2 uv0 = {0.f, 0.f};
		vertex2 uv1 = {0.f, 0.f};
		float palette = 0;
		float x = 0.f;
		float y = 0.f;
		uint8_t attrib = 0; // blend mode here
	};

	struct tilemap;
	struct tilemap_cmd : draw_cmd {
		tilemap *tile;
		float x = 0.f;
		float y = 0.f;
	};

	template<typename T>
	struct draw_list {
		float width;
		float height;
		std::vector<T> list;
	};

	extern texture canvas_texture[8];
	extern bgfx::IndexBufferHandle quad_indexbuf;
	extern bgfx::UniformHandle s_texture;
	extern bgfx::UniformHandle s_resolution;
	extern vec2_16 translate;

	extern std::unordered_map<std::string, texture *> texture_map;
	extern std::unordered_map<std::string, texture> texture_file_map;

	// TODO: x text rendering
	void	render_init(void);
	void	render(uint8_t _view_id = 0);
	void	draw_emulator(float x = 0.f, float y = 0.f, float alpha = 1.f, int view = 1);
	void	flush_lines(void);
	void	flush_rects(void);
	void	flush_primitives(void);
	void	flush(void);
	void	set_canvas_size(uint8_t id, uint16_t width, uint16_t height);
	void	set_color(float r, float g, float b, float a = 1.0f);
	//void	set_palette(sol::table dst, sol::table src);
	//void	set_palette(std::vector<vertex3> dst, std::vector<vertex3> src);
	void	set_palette(uint8_t n);
	void	set_palette(void);
	//uint8_t	push_palette(uint8_t r, uint8_t g, uint8_t b);
	uint8_t	push_palette(sol::table values);
	void	reset_blend(void);
	void	set_blend(int blend);
	void	draw_line(float x1, float y1, float x2, float y2);
	void	draw_rect(float x, float y, float w, float h);
	void	draw_rect_line(float x, float y, float w, float h);
	void	draw_sprite(texture &_texture, float x, float y, uint8_t attrib = 0);
	void	draw_sprite(texture *_texture, float x, float y, uint8_t attrib = 0);
	void	draw_sprite(std::string id, float x, float y, uint8_t attrib = 0);
	void	draw_subsprite(texture &_texture, std::string subid, float x, float y, uint8_t attrib = 0);
	void	draw_subsprite(texture *_texture, std::string subid, float x, float y, uint8_t attrib = 0);
	void	draw_subsprite(std::string id, std::string subid, float x, float y, uint8_t attrib = 0);
	void	draw_sprite_masked(texture &_texture, float x, float y, uint8_t attrib = 0);
	void	draw_tilemap(tilemap *tile, float x, float y);
	void	set_mask(texture &_texture);
	void	set_mask_uv(float uv0x, float uv0y, float uv1x, float uv1y);
	texture *get_texture(std::string id);
	texture	&load_texture(std::string filename);
	texture	&load_texture(std::string filename, std::string id);
	texture	&reload_texture(std::string id);
	shader	load_shader(std::string name);
	bool	save_atlas(texture *t, std::string filename = "");
	bool	load_atlas(texture *t, std::string filename = "");
}

#endif
