#ifndef XPP_TILEMAP_HPP
#define XPP_TILEMAP_HPP

#include "renderer.hpp"

namespace xpp::video {
	struct tile_t {
		//union {
			uint8_t x = 0;
			uint8_t y = 0xFF;
			//idx = 0;
		//} tile;
		uint8_t flip = 0;
		uint8_t pad = 0;
	};
	struct chunk_t {

	};
	struct tilemap {
		bgfx::TransientVertexBuffer tvb;
		bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;
		texture tileset;
		texture map_texture;
		sprite_vertex *positions;
		vertex2	*uv_cache;
		tile_t map[14][16];
		uint8_t *map_attrib;
		uint16_t map_width = 16;
		uint16_t map_height = 14;
		int tile_size = 16;
		int buffer_count = 0;
		int buffer_max = 256;
		tilemap() {}
		//void init(texture _tileset, uint16_t _width, uint16_t _height);
		void init(std::string _tileset, uint16_t _width, uint16_t _height);
		void fill(uint8_t *map_data, size_t size = 0);
		void reset(void);
		uint16_t get_tile(uint16_t x, uint16_t y);
		uint16_t get_tile(int index);
		uint8_t get_attrib(uint16_t x, uint16_t y);
		uint8_t get_attrib(int index);
		vertex2 get_tile_uv(uint16_t tile, uint8_t attr = 0);
		ImVec2 get_tile_uv0(uint16_t tile, uint8_t attr = 0);
		ImVec2 get_tile_uv1(uint16_t tile, uint8_t attr = 0);
		void set_tile(uint16_t x, uint16_t y, uint16_t tile, uint8_t attr = 0);
		void set_tile_index(int index, uint16_t tile, uint8_t attr = 0); // set_tile ambiguous sig
		void erase_tile(uint16_t x, uint16_t y);
		void erase_tile_index(int index); // ambiguous sig
		void draw_tile(int x, int y, uint16_t tile, uint8_t attr = 0);
		void draw_block(int x, int y, tile_block_t &block, uint8_t attr = 0);
		vec2_16 tile_index_to_xy(uint16_t tile);
		uint16_t tile_xy_to_index(uint16_t x, uint16_t y);
		vec2_16 map_index_to_xy(int tile);
		int map_xy_to_index(uint16_t x, uint16_t y);
		void draw(float x, float y);
		void flush(float depth);
		//T *next_vertex(bool _flush = true);
	};
}

#endif
