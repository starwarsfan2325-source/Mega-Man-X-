#include "animation.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace xpp {

namespace {

} // namespace

// interface

animation_t::animation_t(void)
{
	sequence_t s;
	seq.push_back(s);
}

animation_t::animation_t(std::string path)
{
	sequence_t s;
	seq.push_back(s);
	load("res/seq/" + path + ".seq");
	reset();
}

animation_t::~animation_t(void)
{
}

// was using >>/<< over .read()/.write()
// caused BIG, HUGE, GIANT issues. >> = garbage out, << = failbit in...

size_t
sequence_t::serialize_v3(std::ofstream &dst)
{
	uint8_t tmp8;
	int16_t tmp16;
	dst << (uint8_t)(frames.size());
	dst << loop_frame;

	// write texture filename and id interleaved
	std::vector<std::string> textures;
	for (auto &i : frames) {
		std::string f = video::get_texture(i.texture_id)->filename;
		if (std::find(textures.begin(), textures.end(), f) == textures.end()) {
			textures.push_back(f);
			textures.push_back(i.texture_id);
		}
	}

	tmp8 = textures.size();
	dst.write((const char *)&tmp8, 1);
	for (auto &i : textures) {
		tmp8 = i.size();
		dst.write((const char *)&tmp8, 1);
		dst.write(i.c_str(), tmp8);
	}

	int n = 0;
	for (auto &i : frames) {
		// remember it's interleaved, id second
		int j;
		for (j = 0; j < textures.size(); j += 2) {
			// if we don't divide we can only hold 128 textures vs. 256
			if (textures[j + 1] == i.texture_id) {
				tmp8 = j / 2;
				dst.write((const char *)&tmp8, 1);
				break;
			}
		}
		// shouldn't happen
		if (j >= textures.size()) {
			printx(CONSOLE_WARN, "Texture ID missing from sequence's texture table!");
			tmp8 = 0;
			dst.write((const char *)&tmp8, 1);
		}
		// TODO: ensure these strings are <256 characters
		tmp8 = i.subtexture_id.size();
		dst.write((const char *)&tmp8, 1);
		dst.write(i.subtexture_id.c_str(), tmp8);
		tmp16 = i.offx;
		dst.write((const char *)&tmp16, 2);
		tmp16 = i.offy;
		dst.write((const char *)&tmp16, 2);
		tmp8 = i.time;
		dst.write((const char *)&tmp8, 1);
		tmp8 = i.attrib;
		dst.write((const char *)&tmp8, 1);
		++n;
		if (n == 256)
			break;
	}
	return n;
}

bool
animation_t::save(std::string filename)
{
	std::ofstream ofs;
	ofs.open(filename, std::ios::binary);
	if (!ofs.is_open()) {
		printx(CONSOLE_ERROR, "Failed to save animation to %s", filename.c_str());
		return false;
	}
	size_t written = 0;
	ofs << 'S' << 'E' << 'Q';
	ofs << (uint8_t)ANIM_VERSION;
	ofs << (uint8_t)(seq.size());
	for (int i = 0; i < seq.size(); ++i) {
		written += seq[i].serialize_v3(ofs);
	}
	printc("Saved animation to %s (%ld written)", filename.c_str(), written);
	ofs.flush();
	ofs.close();
	return true;
}

bool
sequence_t::unserialize_v3(std::ifstream &src)
{
	uint8_t n_frames;
	src.read((char *)&n_frames, 1);
	src.read((char *)&loop_frame, sizeof(loop_frame));
	if (n_frames == 0) {
		printx(CONSOLE_ERROR, "Invalid sequence length (0 frames)");
		return false;
	}
	frames.clear();
	char ch;
	uint8_t size;
	src.read((char *)&size, 1);
	std::string texture_file;
	std::string texture_id;
	if (size % 2 != 0) {
		printx(CONSOLE_ERROR, "Invalid texture table for sequence (uneven size)");
		return false;
	}
	std::vector<std::string> ids;
	for (int i = 0; i < size / 2; ++i) {
		uint8_t strsize;
		texture_file = "";
		src.read((char *)&strsize, 1);
		for (int j = 0; j < strsize; ++j) {
			src.read((char *)&ch, 1);
			texture_file += ch;
		}
		src.read((char *)&strsize, 1);
		texture_id = "";
		for (int j = 0; j < strsize; ++j) {
			src.read((char *)&ch, 1);
			texture_id += ch;
		}
		ids.push_back(texture_id);
		if (texture_file.size() > 4 && texture_id.size() > 0)
			video::load_texture(texture_file, texture_id);
	}
	for (int i = 0; i < n_frames; ++i) {
		frame_t f;

		src.read((char *)&size, 1);
		if (size < ids.size())
			f.texture_id = ids[size];
		else
			f.texture_id = "";
		src.read((char *)&size, 1);
		f.subtexture_id = "";
		for (int j = 0; j < size; ++j) {
			src.read((char *)&ch, 1);
			f.subtexture_id += ch;
		}

		src.read((char *)&f.offx, sizeof(f.offx));
		src.read((char *)&f.offy, sizeof(f.offy));
		src.read((char *)&f.time, sizeof(f.time));
		src.read((char *)&f.attrib, sizeof(f.attrib));

		frames.push_back(std::move(f));
	}
	return true;
}

bool
animation_t::load(std::string filename)
{
	std::ifstream ifs;
	ifs.open(filename, std::ios::binary);
	if (!ifs.is_open()) {
		printx(CONSOLE_ERROR, "Failed to open animation %s", filename.c_str());
		return false;
	}
	ifs.seekg(0, ifs.end);
	size_t len = ifs.tellg();
	ifs.seekg(0, ifs.beg);
	printc("%ld len", len);
	uint8_t tmp;
	ifs >> tmp;
	if (tmp != 'S')
		goto _invalid_header;
	ifs >> tmp;
	if (tmp != 'E')
		goto _invalid_header;
	ifs >> tmp;
	if (tmp != 'Q')
		goto _invalid_header;
	ifs >> tmp;
	if (tmp == 0 || tmp > 32) {
		printx(CONSOLE_ERROR, "Invalid version for animation %s", filename.c_str());
		return false;
	}
	if (tmp > ANIM_VERSION) {
		printx(CONSOLE_ERROR, "Unsupported version for animation %s (%d > %d)",
			filename.c_str(), tmp, ANIM_VERSION);
		return false;
	}
	uint8_t layers;
	ifs >> layers;
	if (layers == 0 || len < (layers * 40) || len > (layers * 64 * 1024))
		goto _invalid_header;
	seq.clear();
	for (int i = 0; i < layers; ++i) {
		sequence_t s;
		seq.push_back(std::move(s));
		if (!seq[i].unserialize_v3(ifs)) {
			printx(CONSOLE_ERROR, "Error reading sequence #%d for animation %s",
				i, filename.c_str());
			ifs.close();
			return false;
		}
		// fix advance on first update (while not playing) bug due to timer being 0
		seq[i].reset();
		seq[i].playing = false;
	}
	ifs.close();
	return true;

_invalid_header:
	printx(CONSOLE_ERROR, "Invalid header for animation %s", filename.c_str());
	return false;
}

void
sequence_t::reset(void)
{
	//playing = true;
	playing = false;
	finished = false;
	frame = 0;
	if (frame < frames.size())
		timer = frames[frame].time;
}

bool
sequence_t::advance(void)
{
	++frame;
	finished = false;
	if (frame >= frames.size()) {
		frame = loop_frame;
		//if (loop_frame == frames.size() - 1)
		finished = true;
		if (frame >= frames.size()) {
			frame = 0;
			return false;
		}
	}
	timer = frames[frame].time;
	return true;
}

bool
sequence_t::update(void)
{
	if (playing)
		timer -= speed;
	if (timer <= 0)
		return advance();
	return false;
}

void
sequence_t::draw(uint16_t x, uint16_t y, uint8_t attrib)
{
	draw_frame(frame, x, y, attrib);
}

void
sequence_t::draw_frame(uint8_t _frame, uint16_t x, uint16_t y, uint8_t attrib)
{
	if (_frame >= frames.size())
		return;
	if (frames[_frame].subtexture_id.size() == 0) {
		video::draw_sprite(
			frames[_frame].texture_id,
			x + frames[_frame].offx,
			y + frames[_frame].offy,
			frames[_frame].attrib ^ attrib
		);
	} else {
		video::draw_subsprite(
			frames[_frame].texture_id,
			frames[_frame].subtexture_id,
			x + frames[_frame].offx,
			y + frames[_frame].offy,
			frames[_frame].attrib ^ attrib
		);
	}
}

void
animation_t::reset(uint8_t layer)
{
	if (layer < seq.size())
		seq[layer].reset();
}
void
animation_t::reset(void)
{
	for (int i = 0; i < seq.size(); ++i)
		seq[i].reset();
}

void
animation_t::pause(uint8_t layer)
{
	if (layer < seq.size())
		seq[layer].playing = false;
}
void
animation_t::pause(void)
{
	for (int i = 0; i < seq.size(); ++i)
		seq[i].playing = false;
}

void
animation_t::play(uint8_t layer)
{
	if (layer < seq.size())
		seq[layer].playing = true;
}
void
animation_t::play(void)
{
	for (int i = 0; i < seq.size(); ++i)
		seq[i].playing = true;
}

bool
animation_t::advance(uint8_t layer)
{
	if (layer < seq.size())
		return seq[layer].advance();
	else
		return false;
}
bool
animation_t::advance(void)
{
	bool adv = false;
	for (int i = 0; i < seq.size(); ++i) {
		if (seq[i].advance())
			adv = true;
	}
	return adv;
}

bool
animation_t::update(uint8_t layer)
{
	if (layer < seq.size())
		return seq[layer].update();
	else
		return false;
}
bool
animation_t::update(void)
{
	bool adv = false;
	for (int i = 0; i < seq.size(); ++i) {
		if (seq[i].update())
			adv = true;
	}
	return adv;
}

uint8_t
animation_t::length(uint8_t layer)
{
	if (layer < seq.size())
		return seq[layer].frames.size();
	else
		return 0;
}
uint8_t
animation_t::length(void)
{
	return seq[0].frames.size();
}

uint8_t
animation_t::frame(uint8_t layer)
{
	if (layer < seq.size())
		return seq[layer].frame;
	else
		return 0;
}
uint8_t
animation_t::frame(void)
{
	return seq[0].frame;
}

bool
animation_t::finished(uint8_t layer)
{
	if (layer < seq.size())
		return seq[layer].finished;
	else
		return 0;
}
bool
animation_t::finished(void)
{
	return seq[0].finished;
}

void
animation_t::draw(uint16_t x, uint16_t y, uint8_t attrib)
{
	for (int i = 0; i < seq.size(); ++i)
		seq[i].draw(x, y, attrib);
}

void
animation_t::draw_layer(uint8_t layer, uint16_t x, uint16_t y, uint8_t attrib)
{
	if (layer < seq.size())
		seq[layer].draw(x, y, attrib);
}


} // namespace
