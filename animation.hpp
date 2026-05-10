#ifndef XPP_ANIMATION_HPP
#define XPP_ANIMATION_HPP

#include "renderer.hpp"

#define ANIM_LOOP (0)
#define ANIM_ONCE (1)
#define ANIM_PING_PONG (2)

#define ANIM_VERSION (3)

namespace xpp {
	struct frame_t {
		std::string texture_id = "missing";
		std::string subtexture_id = "";
		int16_t offx = 0, offy = 0;
		uint8_t time = 1;
		uint8_t attrib = 0;
	};

	struct sequence_t {
		std::vector<frame_t> frames;
		bool playing = false;
		bool has_advanced = false;
		int16_t timer = 0;
		uint8_t loop_frame = 0;
		uint8_t frame = 0;
		uint8_t speed = 1;
		bool finished = false;

		sequence_t(void) {
			frame_t blank;
			blank.texture_id = "missing";
			frames.push_back(std::move(blank));
		}
		~sequence_t(void) {}
		size_t serialize_v3(std::ofstream &dst);
		bool unserialize_v3(std::ifstream &src);
		void reset(void);
		bool advance(void);
		bool update(void);
		void draw(uint16_t x, uint16_t y, uint8_t attrib = 0);
		void draw_frame(uint8_t frame, uint16_t x, uint16_t y, uint8_t attrib = 0);
	};

	struct animation_t {
		std::vector<sequence_t> seq;
		std::string id = "default";
		bool playing = false;
		bool destruct = false;

		animation_t(void);
		animation_t(std::string path);
		~animation_t(void);
		bool save(std::string filename);
		bool load(std::string filename);
		void reset(uint8_t layer);
		void reset(void);
		void play(uint8_t layer);
		void play(void);
		void pause(uint8_t layer);
		void pause(void);
		bool advance(uint8_t layer);
		bool advance(void);
		bool update(uint8_t layer);
		bool update(void);
		uint8_t length(uint8_t layer);
		uint8_t length(void);
		uint8_t frame(uint8_t layer);
		uint8_t frame(void);
		bool finished(uint8_t layer);
		bool finished(void);
		void draw(uint16_t x, uint16_t y, uint8_t attrib = 0);
		void draw_layer(uint8_t layer, uint16_t x, uint16_t y, uint8_t attrib = 0);
		//void get_frame(uint8_t layer, uint8_t frame);
	};
}

#endif
