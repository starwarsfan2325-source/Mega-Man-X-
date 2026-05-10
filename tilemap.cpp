#include "tilemap.hpp"
#include <stdio.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <unordered_map>
#include "stb_image.h"

namespace xpp::video {

namespace {

bool inited = false;
bgfx::VertexLayout layout_tilemap;
bgfx::UniformHandle s_map;
bgfx::UniformHandle s_tile_info;
shader tilemap_shader;

const static float tilemap_vertices[20] = {
	//x		y		z		uv
	0.f,	0.f,	0.f,	0.0f, 0.0f,
	0.f,	224.f,	0.f,	0.0f, 1.0f,
	256.f,	224.f,	0.f,	1.0f, 1.0f,
	256.f,	0.f,	0.f,	1.0f, 0.0f,
};

void
tilemap_init(void)
{
	inited = true;
	layout_tilemap.begin()
				  .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				  .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
				  .end();
	tilemap_shader = load_shader("tilemap");
	s_map = bgfx::createUniform("s_map", bgfx::UniformType::Sampler);
	s_tile_info = bgfx::createUniform("s_tile_info", bgfx::UniformType::Vec4);
}

// combine flipped tiles into one tall texture for the shader
texture
tilemap_load_texture(std::string filename)
{
	unsigned char *img;
	int w, h, chan, size, tile_pitch, n_tiles, t;
	texture _texture;
	uint8_t* data;

	if (!std::filesystem::exists(filename))
		goto _failure;

	img = stbi_load(filename.c_str(), &w, &h, &chan, STBI_rgb_alpha);
	if (img == NULL)
		goto _failure_stb;
	if (w % 16 != 0 || h % 16 != 0) {
		printx(CONSOLE_ERROR, "Tileset size is not a multiple of 16");
		goto _failure_stb;
	}

	size = w * h * 4;
	data = (uint8_t *)calloc(w * h * 4 * 4, 1);
	memcpy(data, img, size);
	tile_pitch = w * 4;
	n_tiles = (w * h) / 16;
	// black magic do not touch
	// hflip
	data += size;
	for (t = 0; t < w / 16; ++t) {
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < 16; ++x) {
				int xx = ((w - (x + 1)) + ((t + 1) * 16));
				int index = ((y * w) + x + (t * 16)) * 4;
				int index2 = (((y - 1) * w) + xx) * 4;
				data[index + 0] = img[index2 + 0];
				data[index + 1] = img[index2 + 1];
				data[index + 2] = img[index2 + 2];
				data[index + 3] = img[index2 + 3];
			}
		}
	}
	// vflip
	data += size;
	for (t = 0; t < h / 16; ++t) {
		for (int y = 0; y < 16; ++y) {
			for (int x = 0; x < w; ++x) {
				int yy = ((t + 1) * 16) - (y + 1);
				int index = ((y * w) + x + (t * 16 * w)) * 4;
				int index2 = ((yy * w) + x) * 4;
				data[index + 0] = img[index2 + 0];
				data[index + 1] = img[index2 + 1];
				data[index + 2] = img[index2 + 2];
				data[index + 3] = img[index2 + 3];
			}
		}
	}
	// hvflip (re-use vflip data here, just hflip it)
	data += size;
	for (t = 0; t < w / 16; ++t) {
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < 16; ++x) {
				int xx = ((w - (x + 1)) + ((t + 1) * 16));
				int index = ((y * w) + x + (t * 16)) * 4;
				int index2 = (((y - 1) * w) + xx) * 4;
				data[index + 0] = (data - size)[index2 + 0];
				data[index + 1] = (data - size)[index2 + 1];
				data[index + 2] = (data - size)[index2 + 2];
				data[index + 3] = (data - size)[index2 + 3];
			}
		}
	}
	data -= size * 3;

	printf("%d, %d | %ld\n", w, h, size_t(w * h * 4 * 4));
	_texture.handle = bgfx::createTexture2D(
		(uint16_t)w,
		(uint16_t)h * 4,
		false,
		1,
		bgfx::TextureFormat::RGBA8,
		BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
		bgfx::copy(data, w * h * 4 * 4)
	);

	stbi_image_free(img);

	if (!bgfx::isValid(_texture.handle))
		goto _failure;

	_texture.width = w;
	_texture.height = h; // ???
	_texture.filename = filename;
	return _texture;

_failure_stb:
	stbi_image_free(img);
_failure:
	printx(CONSOLE_ERROR, "Failed to load map texture: %s", filename.c_str());
	_texture.filename = "";
	_texture.handle = video::texture_map["invalid"]->handle;
	_texture.width = 16;
	_texture.height = 16;
	return _texture;
}

} // namespace

// interface

texture
create_map_texture(uint8_t* map_ptr, int width = 16, int height = 14)
{
	uint32_t size = width * height * 4;
	texture _texture;
	_texture.filename = "MAP";
	_texture.width = width;
	_texture.height = height;
	_texture.handle = bgfx::createTexture2D(
		(uint16_t)width,
		(uint16_t)height,
		false,
		1,
		bgfx::TextureFormat::RGBA8,
		BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
		NULL); // dynamic texture

	bgfx::updateTexture2D(_texture.handle, 1, 0, 0, 0, width, height,
		bgfx::makeRef(map_ptr, size)
	);

	return _texture;
}

void
tilemap::init(std::string _tileset, uint16_t _width, uint16_t _height)
{
	if (!inited)
		tilemap_init();

	//tileset = _tileset;
	//tileset = tilemap_load_texture("res/seahorse.png");
	tileset = tilemap_load_texture(_tileset);
	map_width = _width;
	map_height = _height;
	for (int y = 0; y < 14; ++y) {
		for (int x = 0; x < 16; ++x) {
			/*map[y][x][0] = 0;
			map[y][x][1] = 0;
			map[y][x][2] = 0;
			map[y][x][3] = 0;*/
		}
	}
	map_attrib = (uint8_t *)calloc(map_width * map_height, 4);
	map_texture = create_map_texture((uint8_t*)&map[0][0], 16, 14);
	fill(NULL);
	int z = 0;
	for (int i = 0; i < map_width * map_height * 4; i += 4) {
		//map[i] = i / 4;
	}
}

void
tilemap::fill(uint8_t *map_data, size_t size)
{
	if (map_data == NULL) {
		for (int y = 0; y < map_height; ++y) {
			for (int x = 0; x < map_width; ++x) {
				//map[y][x].x = (uint8_t)size;
			}
		}
	}
}

void tilemap::reset(void){}

// here

void
tilemap::draw(float x, float y)
{
	draw_tilemap(this, x, y);
}

void
tilemap::flush(float depth)
{
	bgfx::updateTexture2D(map_texture.handle, 1, 0, 0, 0, map_width, map_height,
		bgfx::makeRef(&map[0][0], map_width * map_height * 4)
	);
	if (!bgfx::isValid(tileset.handle) || !bgfx::isValid(map_texture.handle)) {
		printx(CONSOLE_ERROR, "Tried to draw tileset with invalid texture");
		return;
	}
	if (!bgfx::isValid(tilemap_shader.program)) {
		printx(CONSOLE_ERROR, "Tried to draw tileset with invalid shader");
		return;
	}

	bgfx::allocTransientVertexBuffer(&tvb, 4, layout_tilemap);

	// TODO FIXME PLEASE THIS IS REALLY BAD
	float *vertices = (float *)tvb.data;
	memcpy(vertices, tilemap_vertices, sizeof(float) * 20);
	vertices[2] = depth;
	vertices[7] = depth;
	vertices[12] = depth;
	vertices[17] = depth;
	bgfx::setVertexBuffer(0, &tvb);
	bgfx::setIndexBuffer(quad_indexbuf, 0, 6);
	bgfx::setTexture(0, s_texture, tileset.handle);
	bgfx::setTexture(1, s_map, map_texture.handle);
	float info[4] = {
		tileset.width / 16.f, tileset.height / 16.f,
		0.f, 0.f
	};
	bgfx::setUniform(s_tile_info, &info, 1);

	bgfx::setState(BGFX_STATE_DEFAULT | ALPHA_STATE);
	bgfx::submit(view_id, tilemap_shader.program);
}

//

void tilemap::draw_tile(int x, int y, uint16_t tile, uint8_t attr){}

void tilemap::draw_block(int x, int y, tile_block_t &block, uint8_t attr){}

uint16_t
tilemap::get_tile(uint16_t x, uint16_t y)
{
	return map[y][x].x;
}

uint16_t
tilemap::get_tile(int index)
{
	return map[0][0].x;
} // FIXME

uint8_t tilemap::get_attrib(uint16_t x, uint16_t y){return map_attrib[(y * map_width) + x];}

uint8_t tilemap::get_attrib(int index) {return map_attrib[0];} // FIXME

void
tilemap::set_tile(uint16_t x, uint16_t y, uint16_t tile, uint8_t attr)
{
	if (x >= map_width || y >= map_height)
		return;
	map[y][x].x = tile_index_to_xy(tile).x;
	map[y][x].y = tile_index_to_xy(tile).y;
	map[y][x].flip = attr;
	//map_attrib[(y * map_width) + x] = attr;
}

void
tilemap::set_tile_index(int index, uint16_t tile, uint8_t attr)
{
	//printx(CONSOLE_WARN, "set_tile_index unimplemented");
	/*if (index >= map_width * map_height)
		return;
	map[index] = tile;
	map_attrib[index] = attr;*/
}

void
tilemap::erase_tile(uint16_t x, uint16_t y)
{
	if (x >= map_width || y >= map_height)
		return;
	map[y][x].x = 0;
	map[y][x].y = 0xFF;
	map[y][x].flip = 0;
	//map_attrib[(y * map_width) + x] = attr;
}

void
tilemap::erase_tile_index(int index)
{
	printx(CONSOLE_WARN, "erase_tile_index unimplemented");
	/*if (index >= map_width * map_height)
		return;
	map[index] = tile;
	map_attrib[index] = attr;*/
}

vec2_16
tilemap::tile_index_to_xy(uint16_t tile)
{
	return {
		uint16_t(tile % (uint16_t(tileset.width) / 16)),
		uint16_t(tile / (uint16_t(tileset.width) / 16))
	};
}

uint16_t
tilemap::tile_xy_to_index(uint16_t x, uint16_t y)
{
	return (y * (tileset.width / 16)) + x;
}

vec2_16
tilemap::map_index_to_xy(int index)
{
	return {
		uint16_t(index % map_width),
		uint16_t(index / map_width)
	};
}

int tilemap::map_xy_to_index(uint16_t x, uint16_t y){return (y * map_width) + x;}

} // namespace

