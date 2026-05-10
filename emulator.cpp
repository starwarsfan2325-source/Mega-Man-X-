#include "emulator.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include <dlfcn.h>

#include "input.hpp"
#include "core.hpp"
#include "video.hpp"
#include "event.hpp"

using namespace xpp;

namespace xpp::emulator {

int16_t sndbuf[2] = { 0 };
int16_t *audio_log;
int16_t audio_counter = 0;

bool focused = false;
bool running = false;
bool mute = true;
int run_frames = 0;
std::string rom = "empty";

namespace {

uint8_t *wram;
uint8_t *vram;

}

struct {
	bgfx::TextureHandle tex;
	uint32_t pitch;
	int32_t tex_w, tex_h;
	uint32_t clip_w, clip_h;

	bgfx::TextureFormat::Enum pixfmt;
	uint32_t pixtype;
	uint32_t bpp;
} g_video;

struct {
	void *handle;
	bool initialized;

	void (*retro_init)(void);
	void (*retro_deinit)(void);
	unsigned (*retro_api_version)(void);
	void (*retro_get_region)(void);
	void (*retro_get_system_info)(struct retro_system_info *info);
	void (*retro_get_system_av_info)(struct retro_system_av_info *info);
	void (*retro_set_controller_port_device)(unsigned port, unsigned device);
	void (*retro_reset)(void);
	void (*retro_run)(void);
	size_t (*retro_serialize_size)(void);
	bool (*retro_serialize)(void *data, size_t size);
	bool (*retro_unserialize)(const void *data, size_t size);
//	void retro_cheat_reset(void);
//	void retro_cheat_set(unsigned index, bool enabled, const char *code);
	bool (*retro_load_game)(const struct retro_game_info *game);
//	bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info);
	void (*retro_unload_game)(void);
//	unsigned retro_get_region(void);
	void *(*retro_get_memory_data)(unsigned id);
	size_t (*retro_get_memory_size)(unsigned id);
} g_retro;

SDL_Window *g_win = NULL;
snd_pcm_t *g_pcm = NULL;
float g_scale = 3;

static float g_vertex[] = {
	-1.0f, -1.0f, // left-bottom
	-1.0f,  1.0f, // left-top
	 1.0f, -1.0f, // right-bottom
	 1.0f,  1.0f, // right-top
};
static float g_texcoords[] ={
	0.0f,  1.0f,
	0.0f,  0.0f,
	1.0f,  1.0f,
	1.0f,  0.0f,
};

#define load_sym(V, S) do {\
	if (!((*(void**)&V) = dlsym(g_retro.handle, #S))) \
		die("Failed to load symbol '" #S "'': %s", dlerror()); \
	} while (0)
#define load_retro_sym(S) load_sym(g_retro.S, S)

#define set_sym(V, S) (*(void**)&V) = (void*)S;
#define set_retro_sym(S) set_sym(g_retro.S, S)

static void
die(const char *fmt, ...)
{
	char buffer[4096];

	va_list va;
	va_start(va, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, va);
	va_end(va);

	fputs(buffer, stderr);
	fputc('\n', stderr);
	fflush(stderr);

	exit(EXIT_FAILURE);
}

static void
resize_to_aspect(double ratio, int sw, int sh, int *dw, int *dh)
{
	*dw = sw;
	*dh = sh;

	if (ratio <= 0)
		ratio = (double)sw / sh;

	if ((float)sw / sh < 1)
		*dw = *dh * ratio;
	else
		*dh = *dw / ratio;
}

static void
video_configure(const struct retro_game_geometry *geom)
{
	int nwidth, nheight;

	nwidth *= g_scale;
	nheight *= g_scale;
	nwidth = video::width();
	nheight = video::height();

	if (!g_win)
		panic("No window passed to emulator");

	if (bgfx::isValid(g_video.tex))
		bgfx::destroy(g_video.tex);

	g_video.tex = BGFX_INVALID_HANDLE;
	g_video.pixfmt = bgfx::TextureFormat::Enum::BGRA8;

	g_video.tex = bgfx::createTexture2D(
		(uint16_t)512,
		(uint16_t)224,
		false,
		1,
		g_video.pixfmt,
		BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT,
		NULL
	);

	if (!bgfx::isValid(g_video.tex))
		panic("Failed to create the video texture");

	g_video.pitch = geom->base_width * g_video.bpp;

	g_video.tex_w = geom->max_width;
	g_video.tex_h = geom->max_height;
	g_video.clip_w = geom->base_width;
	g_video.clip_h = geom->base_height;
}

static bool
video_set_pixel_format(unsigned format)
{
	if (bgfx::isValid(g_video.tex)) {
		printx(CONSOLE_WARN, "Tried to change pixel format after initialization.");
	}

	return true;
}

static void
video_refresh(const void *data, unsigned width, unsigned height, unsigned pitch)
{
	bgfx::updateTexture2D(g_video.tex, 1, 0, 0, 0, 512, 224,
		bgfx::makeRef(data, 512 * 224 * 4), pitch
	);
}

static void
video_deinit()
{
	if (bgfx::isValid(g_video.tex))
		bgfx::destroy(g_video.tex);
}

static void
audio_init(int frequency)
{
	int err;

	if ((err = snd_pcm_open(&g_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0)
		die("Failed to open playback device: %s", snd_strerror(err));

	err = snd_pcm_set_params(g_pcm, SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, 2, frequency, 1, 64 * 1000);

	if (err < 0)
		die("Failed to configure playback device: %s", snd_strerror(err));
}

static void
audio_deinit()
{
	snd_pcm_close(g_pcm);
}

static size_t
audio_write(const void *buf, unsigned frames)
{
	int written = snd_pcm_writei(g_pcm, buf, frames);

	if (written < 0) {
		snd_pcm_recover(g_pcm, written, 1); // 1 silent

		return 0;
	}

	memcpy((void *)audio_log + (audio_counter * 2), buf, frames * sizeof(int16_t));
	audio_counter += frames;

	return written;
}

static void
core_log(enum retro_log_level level, const char *fmt, ...)
{
	char buffer[4096] = {0};
	static const char * levelstr[] = { "dbg", "inf", "wrn", "err" };
	va_list va;

	va_start(va, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, va);
	va_end(va);

	if (level == 0)
		return;

	fprintf(stderr, "[%s] %s", levelstr[level], buffer);
	fflush(stderr);
	printc("[%s] %s", levelstr[level], buffer);

	if (level == RETRO_LOG_ERROR)
		exit(EXIT_FAILURE);
}

static bool
core_environment(unsigned cmd, void *data)
{
	bool *bval;

	switch (cmd) {
	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
		struct retro_log_callback *cb = (struct retro_log_callback *)data;
		cb->log = core_log;
		break;
	}
	case RETRO_ENVIRONMENT_GET_CAN_DUPE:
		bval = (bool*)data;
		*bval = true;
		break;
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
		const enum retro_pixel_format *fmt = (enum retro_pixel_format *)data;

		if (*fmt > RETRO_PIXEL_FORMAT_RGB565)
			return false;

		return video_set_pixel_format(*fmt);
	}
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char **)data = ".";
        return true;

	default:
		core_log(RETRO_LOG_DEBUG, "Unhandled env #%u", cmd);
		return false;
	}

	return true;
}

static void
core_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch)
{
	if (data)
		video_refresh(data, width, height, pitch);
}

static void
core_input_poll(void)
{
	//if (!focused && !config::background_input)
		//return;
}

static int16_t
core_input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
	if (!focused || port || index || device != RETRO_DEVICE_JOYPAD)
		return 0;

	if(input::getjoy(id))
		return 1;
	else
		return 0;
}

static void
core_audio_sample(int16_t left, int16_t right)
{
	sndbuf[0] = left;
	sndbuf[1] = right;
	audio_write(sndbuf, 1);
}

static size_t
core_audio_sample_batch(const int16_t *data, size_t frames)
{
	return audio_write(data, frames);
}

static void
core_load_static(const char *afile)
{
	void (*set_environment)(retro_environment_t) = NULL;
	void (*set_video_refresh)(retro_video_refresh_t) = NULL;
	void (*set_input_poll)(retro_input_poll_t) = NULL;
	void (*set_input_state)(retro_input_state_t) = NULL;
	void (*set_audio_sample)(retro_audio_sample_t) = NULL;
	void (*set_audio_sample_batch)(retro_audio_sample_batch_t) = NULL;

	memset(&g_retro, 0, sizeof(g_retro));
	g_retro.handle = NULL;

	// these are defined by the core we statically link to

	set_retro_sym(retro_init);
	set_retro_sym(retro_deinit);
	set_retro_sym(retro_api_version);
	set_retro_sym(retro_get_system_info);
	set_retro_sym(retro_get_system_av_info);
	set_retro_sym(retro_set_controller_port_device);
	set_retro_sym(retro_reset);
	set_retro_sym(retro_run);
	set_retro_sym(retro_load_game);
	set_retro_sym(retro_unload_game);
	//load_retro_sym(retro_get_region);
	set_retro_sym(retro_get_memory_data);
	set_retro_sym(retro_get_memory_size);
	set_retro_sym(retro_serialize_size);
	set_retro_sym(retro_serialize);
	set_retro_sym(retro_unserialize);

	set_sym(set_environment, retro_set_environment);
	set_sym(set_video_refresh, retro_set_video_refresh);
	set_sym(set_input_poll, retro_set_input_poll);
	set_sym(set_input_state, retro_set_input_state);
	set_sym(set_audio_sample, retro_set_audio_sample);
	set_sym(set_audio_sample_batch, retro_set_audio_sample_batch);

	set_environment(core_environment);
	set_video_refresh(core_video_refresh);
	set_input_poll(core_input_poll);
	set_input_state(core_input_state);
	set_audio_sample(core_audio_sample);
	set_audio_sample_batch(core_audio_sample_batch);

	g_retro.retro_init();
	g_retro.initialized = true;

	// from load_rom, goes here now because opengl bugs
	struct retro_system_av_info av = {0};
	g_retro.retro_get_system_av_info(&av);
	video_configure(&av.geometry);
	audio_init(av.timing.sample_rate);

	puts("Core loaded (static)");
	printc("Core loaded (static)", afile);
}

static void
core_unload()
{
	if (g_retro.initialized)
		g_retro.retro_deinit();

	if (g_retro.handle)
		dlclose(g_retro.handle);
}

// interface

void
init(const char *core)
{
	audio_log = (int16_t *)calloc(2, 65536);
	set_window(video::window);
	core_load_static(core);
}

void
load_rom(const char *filename)
{
	struct retro_system_av_info av = {0};
	struct retro_system_info system = {0};
	struct retro_game_info info = { filename, 0 };
	FILE *file = fopen(filename, "rb");

	if (!file)
		goto z_error;

	fseek(file, 0, SEEK_END);
	info.size = ftell(file);
	rewind(file);

	g_retro.retro_get_system_info(&system);

	if (!system.need_fullpath) {
		info.data = malloc(info.size);

		if (!info.data || !fread((void*)info.data, info.size, 1, file))
			goto z_error;
	}
	fclose(file);

	if (!g_retro.retro_load_game(&info))
		panic("The core failed to load the content.");

	g_retro.retro_get_system_av_info(&av);

	if (rom == "empty") {
		video_configure(&av.geometry);
		audio_init(av.timing.sample_rate);
	}

	rom = std::string(filename);

	emulator::run();

	wram = (uint8_t *)retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	vram = (uint8_t *)retro_get_memory_data(RETRO_MEMORY_VIDEO_RAM);

	SuperFamicom::ppu.bg_enable(0, true);
	SuperFamicom::ppu.bg_enable(1, true);
	SuperFamicom::ppu.bg_enable(2, true);
	SuperFamicom::ppu.bg_enable(3, true);

	event::fire<std::string>("sfc.load_rom", rom);

	return;

z_error:
	panic("Failed to load cartridge %s", filename);
}

void
set_window(SDL_Window *window)
{
	g_win = window;
}

void
pause(void)
{
	if (running) {
		running = false;
		event::fire("sfc.pause");
	}
}

void
resume(void)
{
	if (!running) {
		run_frames = 0;
		running = true;
		event::fire("sfc.resume");
	}
}

void
set_running(bool r)
{
	if (running && !r) {
		event::fire("sfc.pause");
	} else if (!running && r) {
		run_frames = 0;
		event::fire("sfc.resume");
	}

	running = r;
}

void
run(void)
{
	audio_counter = 0;
	memset(audio_log, 0, 1024);
	// neccesary to preserve sync when muted
	for (int i = 0; i < 8; ++i) 
		SuperFamicom::dsp.channel_enable(i, !mute);
	g_retro.retro_run();
}

bgfx::TextureHandle
get_texture(void)
{
	return g_video.tex;
}

void *
get_ram(void)
{
	return (void *)wram;
}

size_t
get_ram_size(void)
{
	return retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
}

void *
get_vram(void)
{
	return (void *)vram;
}

size_t
get_vram_size(void)
{
	return retro_get_memory_size(RETRO_MEMORY_VIDEO_RAM);
}

void *
get_palette(void)
{
	return retro_get_memory_data(69);
}

size_t
get_palette_size(void)
{
	return retro_get_memory_size(69);
}

bool
save_state(void *data, size_t size)
{
	return retro_serialize(data, size);
}

bool
load_state(const void *data, size_t size)
{
	// lol
	bool ok = retro_unserialize(data, size);
	SuperFamicom::ppu.bg_enable(0, true);
	SuperFamicom::ppu.bg_enable(1, true);
	SuperFamicom::ppu.bg_enable(2, true);
	SuperFamicom::ppu.bg_enable(3, true);
	return ok;
}

size_t
get_state_size(void)
{
	return retro_serialize_size();
}

uint8_t
read_u8(uint16_t address)
{
	return wram[address];
}

int8_t
read_s8(uint16_t address)
{
	return ((int8_t *)(wram))[address];
}

uint16_t
read_u16_le(uint16_t address)
{
	uint8_t a = wram[address];
	uint8_t b = wram[address + 1];
	return ((uint16_t)b << 8) | a;
}

void
write_u8(uint16_t address, uint8_t value)
{
	wram[address] = value;
}

void
cleanup(void)
{
}

} // namespace emulator
