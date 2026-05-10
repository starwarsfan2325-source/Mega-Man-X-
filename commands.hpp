#ifndef XPP_COMMANDS_HPP
#define XPP_COMMANDS_HPP

#include "console.hpp"
#include "emulator.hpp"
#include "input.hpp"

#define NEW_CMD(name) add(#name , name)

namespace xpp::console::command {

static void
help(int argc, std::vector<std::string> argv)
{
	MIN_ARGC(1);
	printc("help test %s", argv[1].c_str());
}

static void
clear(int argc, std::vector<std::string> argv)
{
	console::clear();
}

static void
find(int argc, std::vector<std::string> argv)
{
	// TODO: print conactions
	if (argc > 1) {
		for (auto i : concmds) {
			if (i.name.find(argv[1]) != std::string::npos) {
				printc("%s", i.name.c_str());
			}
		}
	} else {
		for (auto i : concmds) {
			printc("%s", i.name.c_str());
		}
	}
}

static void
bind(int argc, std::vector<std::string> argv)
{
	MIN_ARGC(1);
	if (argc > 2) {
		input::bind(argv[1], argv[2]);
	} else {
		std::string bound = input::get_bind(argv[1]);
		if (bound != "")
			printc("\"%s\" is bound to \"%s\"", argv[1].c_str(), bound.c_str());
		else
			printc("\"%s\" is unbound", argv[1].c_str());
	}
}

static void
unbind(int argc, std::vector<std::string> argv)
{
	MIN_ARGC(1);
	input::unbind(argv[1]);
}

static void
unbindall(int argc, std::vector<std::string> argv)
{
	input::unbindall();
}

static void
sfc_load_rom(int argc, std::vector<std::string> argv)
{
	MIN_ARGC(1);
	emulator::load_rom(argv[1].c_str());
}

static void
sfc_pause(int argc, std::vector<std::string> argv)
{
	emulator::pause();
}

static void
sfc_resume(int argc, std::vector<std::string> argv)
{
	emulator::resume();
}

void
init(void)
{
	NEW_CMD(help);
	NEW_CMD(clear);
	NEW_CMD(find);
	NEW_CMD(bind);
	NEW_CMD(unbind);
	NEW_CMD(unbindall);
	NEW_CMD(sfc_load_rom);
	NEW_CMD(sfc_pause);
	NEW_CMD(sfc_resume); // TODO alias run
}

}

#endif