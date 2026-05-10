#include "renderer.hpp"
#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <filesystem>
#include <fstream>
#include <map>
#include <unordered_map>
#include "emulator.hpp"
#include "tilemap.hpp"
//BGFX_STATE_WRITE_RGB
#define MASK_STATE ( \
		BGFX_STATE_WRITE_A)

// TODO: test on low-end computers or find a way to query max
// should probably divisible by both 4 and 6?
//#define QUAD_INDICES_MAX (2160)
#define QUAD_INDICES_MAX (141160)

// will overflow if ~50k objects are drawn
#define DEPTH_RESET (0.00002f)
#define DEPTH_STEP (0.00002f)

namespace xpp::video {

namespace {

bgfx::VertexLayout layout_sprite;
bgfx::VertexLayout layout_primitive;

// TODO: fix this. i was lazy
static const sprite_vertex sprite_vertices[8] = {
	{{0.0f,	0.0f, 0.f},	{0.0f, 0.0f}, 0.f, {1.0f, 1.0f, 1.0f, 1.0f}},
	{{0.0f,	16.0f, 0.f}, {0.0f, 1.0f}, 0.f, {1.0f, 1.0f, 1.0f, 1.0f}},
	{{16.0f,16.0f, 0.f}, {1.0f, 1.0f}, 0.f, {1.0f, 1.0f, 1.0f, 1.0f}},
	{{16.0f,0.0f, 0.f},	{1.0f, 0.0f}, 0.f, {1.0f, 1.0f, 1.0f, 1.0f}},
};
static const sprite_vertex sprite_vertices_hflip[8] = {
	{{0.0f,	0.0f, 0.f},	{1.0f, 0.0f}, 0.f, {1.0f, 1.0f, 1.0f, 1.0f}},
	{{0.0f,	16.0f, 0.f}, {1.0f, 1.0f}, 0.f, {1.0f, 1.0f, 1.0f, 1.0f}},
	{{16.0f,16.0f, 0.f}, {0.0f, 1.0f}, 0.f, {1.0f, 1.0f, 1.0f, 1.0f}},
	{{16.0f,0.0f, 0.f},	{0.0f, 0.0f}, 0.f, {1.0f, 1.0f, 1.0f, 1.0f}},
};
static const sprite_vertex sprite_vertices_vflip[8] = {
	{{0.0f,	0.0f, 0.f},	{0.0f, 1.0f}, 0.f, {1.0f, 1.0f, 1.0f, 1.0f}},
	{{0.0f,	16.0f, 0.f}, {0.0f, 0.0f}, 0.f, {1.0f, 1.0f, 1.0f, 1.0f}},
	{{16.0f,16.0f, 0.f}, {1.0f, 0.0f}, 0.f, {1.0f, 1.0f, 1.0f, 1.0f}},
	{{16.0f,0.0f, 0.f},	{1.0f, 1.0f}, 0.f, {1.0f, 1.0f, 1.0f, 1.0f}},
};
static const sprite_vertex sprite_vertices_hvflip[8] = {
	{{0.0f,	0.0f, 0.f},	{1.0f, 1.0f}, 0.f, {1.0f, 1.0f, 1.0f, 1.0f}},
	{{0.0f,	16.0f, 0.f}, {1.0f, 0.0f}, 0.f, {1.0f, 1.0f, 1.0f, 1.0f}},
	{{16.0f,16.0f, 0.f}, {0.0f, 0.0f}, 0.f, {1.0f, 1.0f, 1.0f, 1.0f}},
	{{16.0f,0.0f, 0.f},	{0.0f, 1.0f}, 0.f, {1.0f, 1.0f, 1.0f, 1.0f}},
};

static const uint16_t quad_base_indices[6] = {
	0, 1, 2,
	0, 2, 3,
};

uint16_t *quad_indices;

shader sprite_shader;
shader sprite_mask_shader;
shader sprite_add_shader;
shader sprite_pal_shader;
shader primitive_shader;
shader primitive_add_shader;
shader emulator_shader;
texture test, test2;
bgfx::FrameBufferHandle canvas[8];

color_t active_color = {1.f, 1.f, 1.f, 1.f};
vertex3 palette_dst[16];
vertex3 palette_src[16];
bool use_palette = false;
uint8_t palette_counter = 0;

bgfx::ShaderHandle
load_shader_bgfx(std::string filename)
{
	std::ifstream f(filename, std::ios::binary | std::ios::ate);
	if (f.is_open()) {
		size_t size = f.tellg();
		f.seekg(0, f.beg);
		const bgfx::Memory *mem = bgfx::alloc(size + 1);
		f.read((char *)mem->data, size);
		mem->data[size - 1] = '\0';
		return bgfx::createShader(mem);
	} else {
		printx(CONSOLE_ERROR, "Failed to open shader file: %s", filename.c_str());
		return BGFX_INVALID_HANDLE;
	}
}

} // namespace

// interface

uint64_t blend_mode = BGFX_STATE_DEFAULT | ALPHA_STATE;
texture canvas_texture[8];
texture mask_texture;
uint8_t *palette_data;
uint8_t active_palette = 0;
bgfx::TextureHandle palette_texture;
bgfx::IndexBufferHandle quad_indexbuf;
bgfx::UniformHandle s_texture;
bgfx::UniformHandle s_palette;
bgfx::UniformHandle s_texture_mask;
bgfx::UniformHandle s_resolution; // also for translate
vec2_16 translate = {0, 0};
uint16_t view_id = 0;

// switched to pointer texture map because when we did a load_texture
// on an existing texture that used subtextures, it remapped to the
// texture_file_map texture (a separate objetc) that didn't contain
// the subtextures, erasing them
std::unordered_map<std::string, texture *> texture_map;
std::unordered_map<std::string, texture> texture_file_map;

std::unordered_map<uint16_t, draw_list<sprite_cmd>> sprite_list;
std::unordered_map<uint16_t, draw_list<sprite_cmd>> sprite_list_add;
// TODO: generate indexed palette images when loading instead
std::unordered_map<uint16_t, draw_list<sprite_cmd>> sprite_list_palette;
std::vector<rect_cmd> rect_list;
std::vector<rect_cmd> rect_list_add;
std::vector<line_cmd> line_list;
std::vector<tilemap_cmd> tilemap_list;


float depth_counter = DEPTH_RESET;
uint16_t draw_calls = 0;

void
render_init(void)
{
	layout_sprite.begin()
				 .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				 .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
				 .add(bgfx::Attrib::TexCoord1, 1, bgfx::AttribType::Float)
				 .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)				 
				 .end();

	layout_primitive.begin()
					.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
					.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Float)
					.end();

	quad_indices = (uint16_t *)calloc(QUAD_INDICES_MAX, 6 * sizeof(uint16_t));
	int base = 0;
	for (int i = 0; i < QUAD_INDICES_MAX * 6; i += 6) {
		quad_indices[i + 0] = base + quad_base_indices[0];
		quad_indices[i + 1] = base + quad_base_indices[1];
		quad_indices[i + 2] = base + quad_base_indices[2];
		quad_indices[i + 3] = base + quad_base_indices[3];
		quad_indices[i + 4] = base + quad_base_indices[4];
		quad_indices[i + 5] = base + quad_base_indices[5];
		base += 4;
	}
	quad_indexbuf = bgfx::createIndexBuffer(
		bgfx::makeRef(quad_indices, QUAD_INDICES_MAX * sizeof(uint16_t))
	);

	sprite_shader = load_shader("sprite");
	sprite_mask_shader = load_shader("sprite_mask");
	sprite_add_shader = load_shader("sprite_add");
	sprite_pal_shader = load_shader("sprite_pal");
	primitive_shader = load_shader("primitive");
	//primitive_add_shader = load_shader("primitive_add");
	emulator_shader = load_shader("emulator");

	s_texture = bgfx::createUniform("s_texture", bgfx::UniformType::Sampler);
	s_palette = bgfx::createUniform("s_palette", bgfx::UniformType::Sampler);
	s_texture_mask = bgfx::createUniform("s_texture_mask", bgfx::UniformType::Sampler);
	s_resolution = bgfx::createUniform("s_resolution", bgfx::UniformType::Vec4);

	for (uint8_t i = 0; i < 8; ++i) {
		canvas[i] = BGFX_INVALID_HANDLE;
		set_canvas_size(i, 256, 224);
	}

	test = load_texture("res/capsule.png");
	test2 = load_texture("res/x.png");

	// TODO: make this unable to be overwritten!!!
	// TODO: generate this, don't load it
	texture missing = load_texture("res/gfx/common/missing.png", "");
	load_texture("res/gfx/common/missing.png", "missing");
	load_texture("res/gfx/common/missing.png", "default");
	texture *invalid = new texture;
	invalid->width = 16;
	invalid->height = 16;
	invalid->handle = missing.handle;
	texture_map["invalid"] = invalid;
	if (missing.filename == "" || !bgfx::isValid(missing.handle))
		panic("Failed to load res/gfx/common/missing.png");

	texture *t = new texture;
	t->filename = "emulator";
	t->width = 256;
	t->height = 224;
	texture_map["emulator"] = t;

	palette_data = (uint8_t *)calloc(64 * 64, 3);
	if (palette_data == NULL)
		panic("Failed to allocate palette data");
	for (int i = 0; i < 64 * 64 * 3; ++i) {
		palette_data[i] = 0xFF;
		if (i % 3 == 0)
			palette_data[i] = 0x00;
	}
	palette_texture = bgfx::createTexture2D(
		(uint16_t)64,
		(uint16_t)64,
		false,
		1,
		bgfx::TextureFormat::RGB8,
		BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
		NULL
	);

	bgfx::updateTexture2D(palette_texture, 1, 0, 0, 0, 64, 64,
		bgfx::makeRef(palette_data, 64 * 64 * 3)
	);

	set_color(1.f, 1.f, 1.f, 1.f);
}

void
render(uint8_t _view_id)
{
	video::flush(); // reset depth counter
	view_id = _view_id;
	//translate.x = 16;
	// TODO: use multiple views for layers
	bgfx::setViewClear(view_id, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
		0xFF00FFFF, 0.f, 0);
	bgfx::setViewRect(view_id, 0, 0, canvas_texture[view_id].width, canvas_texture[view_id].height);
	bgfx::setViewFrameBuffer(view_id, canvas[view_id]);
	//bgfx::setViewMode(view_id, bgfx::ViewMode::DepthAscending);
	bgfx::touch(view_id);
	// added translate xy
	float resolution[4] = {
		canvas_texture[view_id].width, canvas_texture[view_id].height,
		(float)translate.x, (float)translate.y
	};
	bgfx::setUniform(s_resolution, &resolution, 1);
	video::set_color(1.f, 1.f, 1.f, 1.f);
	set_color(1.f, 1.f, 1.f, 1.f);
	reset_blend();
}

// doesn't seem to blend properly?
void
draw_emulator(float x, float y, float alpha, int view)
{
	depth_counter += DEPTH_STEP;

	set_color(1.f, 1.f, 1.f, alpha);
	bgfx::TransientVertexBuffer tvb;
	bgfx::allocTransientVertexBuffer(&tvb, 4, layout_sprite);

	sprite_vertex *vertices = (sprite_vertex *)tvb.data;
	memcpy(vertices, sprite_vertices, sizeof(sprite_vertex) * 4);
	vertices[0].pos = {x, y, depth_counter};
	vertices[0].color = active_color;
	vertices[1].pos = {x, y + 224.f, depth_counter};
	vertices[1].color = active_color;
	vertices[2].pos = {x + 256.f, y + 224.f, depth_counter};
	vertices[2].color = active_color;
	vertices[3].pos = {x + 256.f, y, depth_counter};
	vertices[3].color = active_color;

	bgfx::setVertexBuffer(0, &tvb, 0, 4);
	bgfx::setIndexBuffer(quad_indexbuf, 0, 6);
	bgfx::setTexture(0, s_texture, emulator::get_texture());

	bgfx::setState(BGFX_STATE_DEFAULT | ALPHA_STATE);
	bgfx::submit(view, emulator_shader.program);
	set_color(1.f, 1.f, 1.f, 1.f);

	texture_map["emulator"]->width = 256;
	texture_map["emulator"]->height = 224;
	texture_map["emulator"]->handle = emulator::get_texture();
}

void
flush_primitives(void)
{
	//flush_lines();
	//flush_rects();
}

// TODO: profile with same draw call for all (original palette) and with separate
void
flush_sprite_list(
	std::unordered_map<uint16_t, draw_list<sprite_cmd>> &list,
	bgfx::ProgramHandle program,
	bgfx::ProgramHandle program_palette,
	uint64_t state)
{
	for (auto &cmd : list) {
		if (cmd.second.list.size() == 0)
			continue;
		bgfx::TransientVertexBuffer tvb;
		bgfx::allocTransientVertexBuffer(&tvb, 4 * cmd.second.list.size(), layout_sprite);
		sprite_vertex *vertices = (sprite_vertex *)tvb.data;
		int n = 0;
		for (auto &i : cmd.second.list) {
			// uvw / uvh should probably be pre-calculated and stored
			float uvw = ((i.uv1.x - i.uv0.x) * cmd.second.width);
			float uvh = ((i.uv1.y - i.uv0.y) * cmd.second.height);
			float palette = i.palette / 4.f; // 256 -> 64
			vertices[n + 0].pos = {i.x, i.y, i.depth};
			vertices[n + 0].palette = palette;
			vertices[n + 0].color = i.color;
			vertices[n + 1].pos = {i.x, i.y + uvh, i.depth};
			vertices[n + 1].palette = palette;
			vertices[n + 1].color = i.color;
			vertices[n + 2].pos = {i.x + uvw, i.y + uvh, i.depth};
			vertices[n + 2].palette = palette;
			vertices[n + 2].color = i.color;
			vertices[n + 3].pos = {i.x + uvw, i.y, i.depth};
			vertices[n + 3].palette = palette;
			vertices[n + 3].color = i.color;
			switch (i.attrib & 0x03) {
			case 0:
				vertices[n + 0].uv = i.uv0;
				vertices[n + 1].uv = {i.uv0.x, i.uv1.y};
				vertices[n + 2].uv = i.uv1;
				vertices[n + 3].uv = {i.uv1.x, i.uv0.y};
				break;
			case 1:
				vertices[n + 0].uv = {i.uv1.x, i.uv0.y};
				vertices[n + 1].uv = i.uv1;
				vertices[n + 2].uv = {i.uv0.x, i.uv1.y};
				vertices[n + 3].uv = i.uv0;
				break;
			case 2:
				vertices[n + 0].uv = {i.uv0.x, i.uv1.y};
				vertices[n + 1].uv = i.uv0;
				vertices[n + 2].uv = {i.uv1.x, i.uv0.y};
				vertices[n + 3].uv = i.uv1;
				break;
			case 3:
				vertices[n + 0].uv = i.uv1;
				vertices[n + 1].uv = {i.uv1.x, i.uv0.y};
				vertices[n + 2].uv = i.uv0;
				vertices[n + 3].uv = {i.uv0.x, i.uv1.y};
				break;
			}
			n += 4;
		}
		bgfx::setVertexBuffer(0, &tvb, 0, 4 * cmd.second.list.size());
		bgfx::setIndexBuffer(quad_indexbuf, 0, 6 * cmd.second.list.size());
		bgfx::setTexture(0, s_texture, {cmd.first});
		if (cmd.second.list[0].attrib & 0x04) {
			bgfx::setTexture(1, s_palette, palette_texture);
			bgfx::setState(state);
			bgfx::submit(view_id, program_palette);
		} else {
			bgfx::setState(state);
			bgfx::submit(view_id, program);
		}
		cmd.second.list.clear();
		++draw_calls;
	}
	list.clear();
}

void
flush_rect_list(
	std::vector<rect_cmd> &list,
	bgfx::ProgramHandle program,
	uint64_t state)
{
	if (list.size() == 0)
		return;
	bgfx::TransientVertexBuffer tvb;
	bgfx::allocTransientVertexBuffer(&tvb, 4 * list.size(), layout_primitive);
	primitive_vertex *vertices = (primitive_vertex *)tvb.data;
	int n = 0;
	for (auto &i : list) {
		vertices[n + 0].pos = {i.x, i.y, i.depth};
		vertices[n + 0].color = i.color;
		vertices[n + 1].pos = {i.x, i.y + i.h, i.depth};
		vertices[n + 1].color = i.color;
		vertices[n + 2].pos = {i.x + i.w, i.y + i.h, i.depth};
		vertices[n + 2].color = i.color;
		vertices[n + 3].pos = {i.x + i.w, i.y, i.depth};
		vertices[n + 3].color = i.color;
		n += 4;
	}
	bgfx::setVertexBuffer(0, &tvb, 0, 4 * list.size());
	bgfx::setIndexBuffer(quad_indexbuf, 0, 6 * list.size());

	bgfx::setState(state);
	bgfx::submit(view_id, program);
	++draw_calls;
	list.clear();
}

void
flush_line_list(
	std::vector<line_cmd> &list,
	bgfx::ProgramHandle program,
	uint64_t state)
{
	if (list.size() == 0)
		return;
	bgfx::TransientVertexBuffer tvb;
	bgfx::allocTransientVertexBuffer(&tvb, 2 * list.size(), layout_primitive);
	primitive_vertex *vertices = (primitive_vertex *)tvb.data;
	int n = 0;
	for (auto &i : list) {
		vertices[n + 0].pos = {i.x1, i.y1, i.depth};
		vertices[n + 0].color = i.color;
		vertices[n + 1].pos = {i.x2, i.y2, i.depth};
		vertices[n + 1].color = i.color;
		n += 2;
	}
	bgfx::setVertexBuffer(0, &tvb, 0, 2 * list.size());

	bgfx::setState(state | BGFX_STATE_PT_LINES);
	bgfx::submit(view_id, program);
	list.clear();
	++draw_calls;
}

void
flush(void)
{
	draw_calls = 0;
	bgfx::touch(view_id);
	bgfx::submit(view_id, primitive_shader.program);
	float resolution[4] = {
		canvas_texture[view_id].width, canvas_texture[view_id].height,
		(float)translate.x, (float)translate.y
	};
	bgfx::setUniform(s_resolution, &resolution, 1);
	bgfx::updateTexture2D(palette_texture, 1, 0, 0, 0, 64, 64,
		bgfx::makeRef(palette_data, 64 * 64 * 4)
	);
	flush_sprite_list(sprite_list, sprite_shader.program,
		sprite_pal_shader.program, BGFX_STATE_DEFAULT | ALPHA_STATE
	);
	flush_sprite_list(sprite_list_add, sprite_add_shader.program, sprite_pal_shader.program, ADD_STATE);
	flush_rect_list(rect_list, primitive_shader.program, BGFX_STATE_DEFAULT | ALPHA_STATE);
	flush_rect_list(rect_list_add, primitive_shader.program, ADD_STATE);
	flush_line_list(line_list, primitive_shader.program, BGFX_STATE_DEFAULT | ALPHA_STATE);
	for (auto i : tilemap_list) {
		i.tile->flush(i.depth);
	}
	tilemap_list.clear();
	depth_counter = DEPTH_RESET;
	palette_counter = 0;
	//printc("DRAW CALLS %d", draw_calls);
}

void
set_canvas_size(uint8_t id, uint16_t width, uint16_t height)
{
	if (width == 0 || height == 0) {
		printx(CONSOLE_ERROR, "Bad canvas size %dx%d", width, height);
		return;
	}
	if (width != canvas_texture[id].width || height != canvas_texture[id].height) {
		if (bgfx::isValid(canvas[id])) {
			flush();
			bgfx::destroy(canvas[id]);
		}

		canvas_texture[id].width = width;
		canvas_texture[id].height = height;
		canvas_texture[id].handle = bgfx::createTexture2D(
			canvas_texture[id].width, canvas_texture[id].height, false, 1,
			bgfx::TextureFormat::RGBA8,
			BGFX_TEXTURE_RT | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT
		);
		// depth texture, will be destroyed next time we are resized
		bgfx::TextureHandle Z = bgfx::createTexture2D(
			width, height,
			false, 1, bgfx::TextureFormat::D24S8,
			BGFX_TEXTURE_RT | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT
		);
		bgfx::Attachment attach[2];
		attach[0].init(canvas_texture[id].handle);
		attach[1].init(Z);
		canvas[id] = bgfx::createFrameBuffer(2, attach, true); // destroy textures
	}
}

void
set_color(float r, float g, float b, float a)
{
	active_color.r = r;
	active_color.g = g;
	active_color.b = b;
	active_color.a = a;
}

void
set_palette(uint8_t n, sol::table values)
{
}

void
set_palette(uint8_t n)
{
	use_palette = true;
	active_palette = n;
}

void
set_palette(std::vector<vertex3> dst, std::vector<vertex3> src)
{
	for (int i = 0; i < dst.size(); ++i)
		palette_dst[i] = dst[i];
	for (int i = 0; i < src.size(); ++i)
		palette_src[i] = src[i];
}

void
set_palette(void)
{
	use_palette = false;
}

/*uint8_t
push_palette(uint8_t r, uint8_t g, uint8_t b)
{
	++palette_counter;
	return palette_counter - 1;
}*/

uint8_t
push_palette(sol::table values)
{
	uint8_t count = values.size();
	for (int i = 1; i <= count; ++i) {
		palette_data[(palette_counter * 64) + i - 1]
			= ((sol::object)values[i]).as<uint8_t>();
	}
	++palette_counter;
	return palette_counter - 1;
}

void
reset_blend(void)
{
	blend_mode = BGFX_STATE_DEFAULT | ALPHA_STATE;
}

void
set_blend(int blend)
{
	if (blend == 0)
		blend_mode = BGFX_STATE_DEFAULT | ALPHA_STATE;
	if (blend == 1)
		blend_mode = MASK_STATE;
	if (blend == 2)
		blend_mode = ADD_STATE;
}

void
draw_line(float x1, float y1, float x2, float y2)
{
	depth_counter += DEPTH_STEP;
	line_cmd cmd;
	cmd.color = active_color;
	cmd.depth = depth_counter;
	cmd.x1 = x1;
	cmd.y1 = y1;
	cmd.x2 = x2;
	cmd.y2 = y2;
	line_list.push_back(std::move(cmd));
}

void
draw_rect(float x, float y, float w, float h)
{
	depth_counter += DEPTH_STEP;
	rect_cmd cmd;
	cmd.color = active_color;
	cmd.depth = depth_counter;
	cmd.x = x;
	cmd.y = y;
	cmd.w = w;
	cmd.h = h;
	rect_list.push_back(std::move(cmd));
}

void
draw_rect_line(float x, float y, float w, float h)
{
	// ?????????????????????????????????
	x += 1;
	w -= 1;
	h -= 1;
	draw_line(x, y, x + w, y);
	draw_line(x + w, y, x + w, y + h);
	draw_line(x + w, y + h, x - 1, y + h);
	draw_line(x - 1, y + h, x, y);
}

void
draw_sprite(texture &_texture, float x, float y, uint8_t attrib)
{
	if (!bgfx::isValid(_texture.handle) || _texture.width == 0) {
		//draw_sprite("missing", x, y, attrib);
		return;
	}
	if (!bgfx::isValid(_texture.handle_palette))
		return;
	depth_counter += DEPTH_STEP;
	sprite_cmd cmd;
	cmd.color = active_color;
	cmd.depth = depth_counter;
	cmd.uv0 = {0.f, 0.f};
	cmd.uv1 = {1.f, 1.f};
	cmd.x = x;
	cmd.y = y;
	cmd.attrib = attrib;
	cmd.id = _texture.filename;
	//if (bgfx::isValid(_texture.handle_palette) && _texture.handle_palette.idx < 4096)
		//cmd.palette = _texture.handle_palette;
	//else
		//cmd.palette = _texture.handle;
	cmd.palette = active_palette;
	if (use_palette) {
		cmd.attrib |= 0x04;
		sprite_list[_texture.handle_idx.idx].width = _texture.width;
		sprite_list[_texture.handle_idx.idx].height = _texture.height;
		sprite_list[_texture.handle_idx.idx].list.push_back(std::move(cmd));
	} else {
		auto &list = (blend_mode == ADD_STATE) ? sprite_list_add : sprite_list;
		list[_texture.handle.idx].width = _texture.width;
		list[_texture.handle.idx].height = _texture.height;
		list[_texture.handle.idx].list.push_back(std::move(cmd));
	}
}

void
draw_sprite(std::string id, float x, float y, uint8_t attrib)
{
	if (texture_map.find(id) != texture_map.end()) {
		draw_sprite(*texture_map[id], x, y, attrib);
	} else {
		draw_sprite(*texture_map["missing"], x, y);
		printx(CONSOLE_WARN, "Tried to draw invalid texture: %s", id.c_str());
	}
}

void
draw_sprite(texture *_texture, float x, float y, uint8_t attrib)
{
	draw_sprite(*_texture, x, y, attrib);
}

void
draw_subsprite(texture &_texture, std::string subid, float x, float y, uint8_t attrib)
{
	if (_texture.subtextures.find(subid) == _texture.subtextures.end()) {
		draw_sprite("missing", x, y, attrib);
		return;
	}
	subtexture &sub = _texture.subtextures[subid];
	depth_counter += DEPTH_STEP;
	sprite_cmd cmd;
	cmd.color = active_color;
	cmd.depth = depth_counter;
	//float uvw = ((sub.uv1.x - sub.uv0.x) * _texture.width);
	//float uvh = ((sub.uv1.y - sub.uv0.y) * _texture.height);
	cmd.uv0 = sub.uv0;
	cmd.uv1 = sub.uv1;
	cmd.x = x;
	cmd.y = y;
	cmd.attrib = attrib;
	auto &list = (blend_mode == ADD_STATE) ? sprite_list_add : sprite_list;
	list[_texture.handle.idx].width = _texture.width;
	list[_texture.handle.idx].height = _texture.height;
	list[_texture.handle.idx].list.push_back(std::move(cmd));
}

void
draw_subsprite(std::string id, std::string subid, float x, float y, uint8_t attrib)
{
	if (texture_map.find(id) != texture_map.end()) {
		draw_subsprite(*texture_map[id], subid, x, y, attrib);
	} else {
		draw_sprite(*texture_map["missing"], x, y);
		printx(CONSOLE_WARN, "Tried to draw invalid texture: %s", id.c_str());
	}
}

void
draw_subsprite(texture *_texture, std::string subid, float x, float y, uint8_t attrib)
{
	draw_subsprite(*_texture, subid, x, y, attrib);
}

void
draw_sprite_masked(texture &_texture, float x, float y, uint8_t attrib)
{
	// TODO static vertices and translate
	bgfx::TransientVertexBuffer tvb;
	bgfx::allocTransientVertexBuffer(&tvb, 4, layout_sprite);

	// TODO FIXME PLEASE THIS IS REALLY BAD
	sprite_vertex *vertices = (sprite_vertex *)tvb.data;
	if ((attrib & 0x01) && (attrib & 0x02))
		memcpy(vertices, sprite_vertices_hvflip, sizeof(sprite_vertex) * 4);
	else if ((attrib & 0x01) && !(attrib & 0x02))
		memcpy(vertices, sprite_vertices_hflip, sizeof(sprite_vertex) * 4);
	else if (!(attrib & 0x01) && (attrib & 0x02))
		memcpy(vertices, sprite_vertices_vflip, sizeof(sprite_vertex) * 4);
	else
		memcpy(vertices, sprite_vertices, sizeof(sprite_vertex) * 4);
	vertices[0].pos = {x, y};
	vertices[0].color = active_color;
	vertices[1].pos = {x, y + _texture.height};
	vertices[1].color = active_color;
	vertices[2].pos = {x + _texture.width, y + _texture.height};
	vertices[2].color = active_color;
	vertices[3].pos = {x + _texture.width, y};
	vertices[3].color = active_color;

	bgfx::setVertexBuffer(0, &tvb, 0, 4);
	bgfx::setIndexBuffer(quad_indexbuf, 0, 6);
	bgfx::setTexture(0, s_texture, _texture.handle);
	if (bgfx::isValid(mask_texture.handle))
		bgfx::setTexture(1, s_texture_mask, mask_texture.handle);

	bgfx::setState(blend_mode);
	// turns out submit has a depth argument wow
	bgfx::submit(view_id, sprite_shader.program);
}

void
draw_tilemap(tilemap *tile, float x, float y)
{
	depth_counter += DEPTH_STEP;
	tilemap_cmd cmd;
	cmd.tile = tile;
	cmd.x = x;
	cmd.y = y;
	cmd.depth = depth_counter;
	tilemap_list.push_back(cmd);
}

void
make_luminance_texture(texture &_texture, unsigned char *img, uint16_t w, uint16_t h)
{
	// should use a huge static buffer for this, don't allocate every time
	unsigned char *data = (unsigned char *)malloc(w * h);
	std::vector<uint32_t> values;
	values.push_back(0); // transparent palette index 0
	uint32_t rgb, a;
	for (int i = 0; i < w * h; ++i) {
		a = (((uint32_t *)img)[i] & 0xFF000000) >> 24;
		if (a < 127) {
			data[i] = 0;
			continue;
		}
		rgb = ((uint32_t *)img)[i] & 0x00FFFFFF;
		auto found = std::find(values.begin(), values.end(), rgb);
		if (found == values.end()) {
			values.push_back(rgb);
			data[i] = values.size() - 1;
		} else {
			data[i] = std::distance(values.begin(), found);
		}
	}
	// TODO: use texture array
	_texture.handle_idx = bgfx::createTexture2D(
		(uint16_t)w,
		(uint16_t)h,
		false,
		1,
		bgfx::TextureFormat::R8,
		BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
		bgfx::copy(data, w * h)
	);
	free(data);
	//uint32_t *data32 = (uint32_t *)calloc(w * h, 4);
	uint32_t *data32 = (uint32_t *)calloc(1024, 4);
	//(1.0f/255)*byte_in; fast
	_texture.palette_size = values.size();
	for (int i = 0; i < _texture.palette_size; ++i) {
		//data[i] = values[i];
		data32[i] = values[i];
	}
	//for (int i = 0; i < 256; ++i)
	//	data32[i] = 0xFFFFFFFF;
	_texture.handle_palette = bgfx::createTexture2D(
		(uint16_t)256, // for UV reasons
		(uint16_t)1,
		false,
		1,
		bgfx::TextureFormat::RGBA8,
		BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
		bgfx::copy(data32, 256 * 4)
	);
	printf("ID %d %s\n", _texture.handle_palette.idx, _texture.filename.c_str());
	free(data32);
	//_texture.handle_palette = _texture.handle;
	/*std::vector<float> values;
	for (int i = 0; i < w * h; ++i) {
		float l = 0.2126 * img[i * 4] + 0.7152 * img[(i * 4) + 1] + 0.0722 * img[(i * 4) + 2];
		if (std::find(values.begin(), values.end(), l) != values.end()) {
			values.push_back(l);
		} else {
			data[i * 2] = l;
			data[(i * 2) + 1] = img[(i * 4) + 3];
		}
	}*/
}

texture *
get_texture(std::string id)
{
	if (texture_map.find(id) != texture_map.end())
		return texture_map[id];
	else
		return texture_map["missing"];
}

texture &
load_texture(std::string filename)
{
	unsigned char *img;
	int w, h, chan;
	texture _texture;

	if (!std::filesystem::exists(filename))
		goto _failure;

	img = stbi_load(filename.c_str(), &w, &h, &chan, STBI_rgb_alpha);

	if (img == NULL)
		goto _failure_stb;

	if (w == 0 || h == 0 || w > 1024 || h > 1024) {
		printx(CONSOLE_ERROR, "Invalid texture size %dx%d (max 1024x1024): %s",
			w, h, filename.c_str());
		goto _failure;
	}

	_texture.handle = bgfx::createTexture2D(
		(uint16_t)w,
		(uint16_t)h,
		false,
		1,
		bgfx::TextureFormat::RGBA8,
		BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
		bgfx::copy(img, w * h * 4)
	);
	make_luminance_texture(_texture, img, w, h);

	stbi_image_free(img);

	if (!bgfx::isValid(_texture.handle))
		goto _failure;

	_texture.width = w;
	_texture.height = h;
	// make filenames consistent - filename table WILL break without
	_texture.filename = std::filesystem::path(filename);
	texture_file_map[_texture.filename] = _texture;

	// TODO: add way to disable automatic atlas loading
	if (std::filesystem::exists(std::filesystem::path(filename).replace_extension(".uv")))
		load_atlas(&texture_file_map[_texture.filename]);

	return texture_file_map[_texture.filename];

_failure_stb:
	stbi_image_free(img);
_failure:
	printx(CONSOLE_ERROR, "Failed to load texture: %s", filename.c_str());
	return *texture_map["invalid"];
}

texture &
load_texture(std::string filename, std::string id)
{
	// TODO: make more efficient
	if (texture_file_map.find(filename) != texture_file_map.end())
		texture_map[id] = &texture_file_map[filename];
	else
		texture_map[id] = &load_texture(filename);
		//printc("Actually loading it %s & %s", filename.c_str(), id.c_str());
	// TODO: return empty texture if invalid
	return *texture_map[id];
}

texture &
reload_texture(std::string id)
{
	unsigned char *img;
	int w, h, chan;
	texture &_texture = *texture_map[id];
	std::string &filename = _texture.filename;

	img = stbi_load(filename.c_str(), &w, &h, &chan, STBI_rgb_alpha);

	if (img == NULL)
		goto _failure_stb;

	if (bgfx::isValid(_texture.handle))
		bgfx::destroy(_texture.handle);

	_texture.handle = bgfx::createTexture2D(
		(uint16_t)w,
		(uint16_t)h,
		false,
		1,
		bgfx::TextureFormat::RGBA8,
		BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
		bgfx::copy(img, w * h * 4)
	);

	stbi_image_free(img);

	if (!bgfx::isValid(_texture.handle))
		goto _failure;

	//bgfx::allocTransientVertexBuffer(&_texture.tvb, 4, layout_sprite); // TODO error check

	_texture.width = w;
	_texture.height = h;
	_texture.filename = std::filesystem::path(filename); // make consistent
	texture_file_map[filename] = _texture;
	return _texture;

_failure_stb:
	stbi_image_free(img);
_failure:
	printx(CONSOLE_ERROR, "Failed to reload texture: %s (%s)", filename.c_str(), id.c_str());
	_texture.filename = "";
	_texture.handle = get_texture("invalid")->handle;
	// prevent issues with 0 w/h textures like setting canvas size to texture size
	_texture.width = 16;
	_texture.height = 16;
	return _texture;
}

void
set_mask(texture &_texture)
{
	mask_texture = _texture;
}

shader
load_shader(std::string name)
{
	std::string ext = "gl"; // default
	switch(bgfx::getRendererType()) {
	case bgfx::RendererType::Vulkan:
		ext = "vk";
		break;
	case bgfx::RendererType::Metal:
		ext = "m";
		break;
	}
	shader _shader = {BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE};
	_shader.fragment = load_shader_bgfx(("res/" + name + ".f" + ext).c_str());
	_shader.vertex = load_shader_bgfx(("res/" + name + ".v" + ext).c_str());
	_shader.program = bgfx::createProgram(_shader.vertex, _shader.fragment, true);
	if (!bgfx::isValid(_shader.program)) {
		printx(CONSOLE_ERROR, "Invalid shader: %s\n", name.c_str());
		printf("Invalid shader: %s\n", name.c_str());
	}
	return _shader;
}

// atlas

bool
save_atlas(texture *t, std::string filename)
{
	if (filename == "") {
		std::filesystem::path p = t->filename;
		p.replace_extension(".uv");
		filename = p;
	}
	std::ofstream ofs;
	ofs.open(filename, std::ios::binary);
	if (!ofs.is_open()) {
		printx(CONSOLE_ERROR, "Failed to save atlas to %s", filename.c_str());
		return false;
	}
	size_t written = 0;
	ofs << 'U' << 'V';
	ofs << (uint8_t)UV_VERSION;
	ofs << (uint8_t)t->subtextures.size();
	uint8_t tmp8;
	float tmpf;
	for (std::pair<std::string, video::subtexture> ii : t->subtextures) {
		video::subtexture &i = ii.second;
		tmp8 = ii.first.size();
		ofs.write((const char *)&tmp8, 1);
		ofs.write(ii.first.c_str(), tmp8);
		ofs.write((const char *)&i, sizeof(subtexture));
		written += 1;
	}
	printc("Saved atlas to %s (%ld written)", filename.c_str(), written);
	ofs.flush();
	ofs.close();
	return true;
}

bool
load_atlas(texture *t, std::string filename)
{
	if (filename == "") {
		std::filesystem::path p = t->filename;
		p.replace_extension(".uv");
		filename = p;
	}
	std::ifstream ifs;
	ifs.open(filename, std::ios::binary);
	if (!ifs.is_open()) {
		printx(CONSOLE_ERROR, "Failed to open atlas %s", filename.c_str());
		return false;
	}
	ifs.seekg(0, ifs.end);
	size_t len = ifs.tellg();
	ifs.seekg(0, ifs.beg);
	printc("%ld len", len);
	uint8_t tmp;
	ifs.read((char *)&tmp, 1);
	if (tmp != 'U')
		goto _invalid_header;
	ifs.read((char *)&tmp, 1);
	if (tmp != 'V')
		goto _invalid_header;
	ifs.read((char *)&tmp, 1);
	if (tmp == 0 || tmp > 32) {
		printx(CONSOLE_ERROR, "Invalid version for atlas %s", filename.c_str());
		return false;
	}
	if (tmp > UV_VERSION) {
		printx(CONSOLE_ERROR, "Unsupported version for atlas %s (%d > %d)",
			filename.c_str(), tmp, UV_VERSION);
		return false;
	}
	uint8_t n_uvs;
	ifs.read((char *)&n_uvs, 1);
	if (n_uvs == 0)
		goto _invalid_header;
	t->subtextures.clear();
	uint8_t size;
	char ch;
	for (int i = 0; i < n_uvs; ++i) {
		subtexture sub;
		ifs.read((char *)&size, 1);
		std::string key = "";
		for (int j = 0; j < size; ++j) {
			ifs.read((char *)&ch, 1);
			key += ch;
		}
		ifs.read((char *)&sub, sizeof(subtexture));
		t->subtextures[key] = std::move(sub);
	}
	ifs.close();
	return true;

_invalid_header:
	printx(CONSOLE_ERROR, "Invalid header for atlas %s", filename.c_str());
	return false;
}

// xfont

xfont::xfont(std::string filename)
{
	atlas = load_texture(filename);
	if (atlas.width > 0.f && atlas.height > 0.f) {
		char_fsize.x = float(char_psize_x) / atlas.width;
		char_fsize.y = float(char_psize_y) / atlas.height;
	}
}

xfont::xfont(std::string filename, char min, char max)
{
	char_min = min;
	char_max = max;
	atlas = load_texture(filename);
	if (atlas.width > 0.f && atlas.height > 0.f) {
		char_fsize.x = float(char_psize_x) / atlas.width;
		char_fsize.y = float(char_psize_y) / atlas.height;
	}
}

void
xfont::draw_char(char ch, float x, float y)
{
	if (!bgfx::isValid(atlas.handle)) {
		draw_sprite("missing", x, y);
		return;
	}

	float offx = char_fsize.x * (ch - char_min);

	depth_counter += DEPTH_STEP;
	sprite_cmd cmd;
	cmd.color = active_color;
	cmd.depth = depth_counter;
	cmd.uv0 = {offx + 0.f, 0.f};
	cmd.uv1 = {offx + char_fsize.x, char_fsize.y};
	cmd.x = x;
	cmd.y = y;
	cmd.attrib = 0;
	auto &list = (blend_mode == ADD_STATE) ? sprite_list_add : sprite_list;
	list[atlas.handle.idx].width = atlas.width;
	list[atlas.handle.idx].height = atlas.height;
	list[atlas.handle.idx].list.push_back(std::move(cmd));
}

void
xfont::draw_text(std::string str, float x, float y)
{
	uint8_t xx = 0;
	for(char &ch : str) {
		if (ch != ' ' && ch != '\n') {
			draw_char(ch, x + (xx * char_psize_x), y);
		} else if (ch == '\n') {
			xx = 0xFF; // will overflow to 0
			y += char_psize_y;
		}
		xx += 1;
	}
}

} // namespace
