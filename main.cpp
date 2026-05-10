#define XPP_LEAN_AND_MEAN 0
#include "video.hpp"
#include "console.hpp"
#include "input.hpp"
#include "sound.hpp"
#include "animation.hpp"
#include "event.hpp"
#include "script.hpp"
#include "editor.hpp"
#include "emulator.hpp"
#include "xgui/xgui.hpp"
#include <unistd.h>

using namespace xpp;

int
main(int argc, char *argv[])
{
	if (!(argc > 1 && strcmp(argv[1], "-nochdir") == 0)) {
		chdir("./game");
	}
	console::init();
	if (argc > 1 && strcmp(argv[1], "-vk") == 0) {
		video::init(bgfx::RendererType::Vulkan);
		printc("Using Vulkan renderer");
	} else {
		video::init();
		printc("Using OpenGL renderer");
	}
	video::render_init();
	input::init();
	sound::init();
	script::init();
	script::load_kernel();
	editor::init();
	emulator::init("../bsnes.so");
	emulator::load_rom("rom/x3.smc");
	emulator::run();
	xgui::init();
	script::load("autoexec.lua");
	script::load("main.lua");
	sol::protected_function lua_boot = lua["x_boot"];
	sol::protected_function lua_update = lua["x_update"];
	sol::protected_function lua_draw = lua["x_draw"];
	script::check_result(lua_boot(), "main.lua");
	emulator::running = true;
	while (1) {
		input::update(); // pumps events
		ImGui_ImplSDL2_NewFrame(video::window);
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			ImGui_ImplSDL2_ProcessEvent(&e);
			if (e.type == SDL_WINDOWEVENT) {
				switch (e.window.event) {
				case SDL_WINDOWEVENT_SIZE_CHANGED:
					video::reset_resize(e.window.data1, e.window.data2);
					break;
				}
			}
			switch (e.type) {
			case SDL_MOUSEWHEEL: {
				ImGuiIO &io = ImGui::GetIO();
				io.MouseWheel = e.wheel.y;
				break;
			}
			case SDL_QUIT:
				goto _cleanup;
			}
		}

		if (emulator::rom != "empty") {
			if (emulator::running) {
				emulator::run();
			} else if (emulator::run_frames > 0) {
				emulator::run();
				--emulator::run_frames;
			}
		}

		// refresh these in case we modify them
		lua_boot = lua["x_boot"];
		lua_update = lua["x_update"];
		lua_draw = lua["x_draw"];
		// don't clear if we aren't rendering
		bool render_game = true;
		if (!lua["game_running"]) {
			int rf = lua["run_frames"];
			if (rf > 0)
				lua["run_frames"] = rf - 1;
			else
				render_game = false;
		}
		if (render_game) {
			script::check_result(lua_update(0.f), "main.lua");
			sound::mix();
			video::render(1);
			script::check_result(lua_draw(), "main.lua");
			video::flush();
		}
		video::render(0);
		editor::update();

		xgui::begin();
		xgui::draw_windows();
		xgui::end();

		bgfx::frame();
	}

_cleanup:
	xgui::cleanup();
	video::cleanup();
	return 0;
}
