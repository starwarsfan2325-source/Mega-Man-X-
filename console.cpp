#include "console.hpp"
#include "commands.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static char printbuf[1024];

void
printc(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vsnprintf(printbuf, sizeof(printbuf), fmt, va);
	va_end(va);

	xpp::console::line_t line = {std::string(printbuf), 0, 0};
	xpp::console::lines.push_back(line);
}

size_t
printx(unsigned char level, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vsnprintf(printbuf, sizeof(printbuf), fmt, va);
	va_end(va);

	xpp::console::line_t line = {std::string(printbuf), 0, level};
	xpp::console::lines.push_back(line);
	return xpp::console::lines.size() - 1;
}

namespace xpp::console {

std::vector<concmd_t>		concmds;
std::vector<conaction_t>	conactions;
std::vector<line_t>			lines;

void
init(void)
{
	printbuf[0] = '\0';
	command::init();

	puts("init");
}

void
loadcfg(std::string filename)
{
	std::ifstream ifs(filename);
	std::string line;

	if (!ifs.is_open()) {
		printc("Failed to open %s", filename.c_str());
		return;
	}

	printc("Executing %s", filename.c_str());
	while(std::getline(ifs, line)) {
		send(line, '>');
	}
}

void
clear(void)
{
	lines.clear();
	printbuf[0] = '\0';
}

bool
send(std::string str, char prompt)
{
	std::istringstream iss(str);
	std::vector<std::string> args{std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{}};
	if (args.size() < 1)
		return false;

	if (str[0] == '#')
		return true;

	// conactions are silent TODO: cvar for this
	if ((str[0] == '+' || str[0] == '-') && str.size() > 1) {
		for (auto &i : conactions) {
			if (str.substr(1) == i.name) {
				i.run(str[0] == '+' ? true : false);
				return true;
			}
		}
	} else {
		for (auto &i : concmds) {
			if (args[0] == i.name) {
				printc("%c %s", prompt, str.c_str());
				i.run(args.size(), args);
				return true;
			}
		}
	}

	printc("%c %s", prompt, str.c_str());
	printc("Unknown command: %s", args[0].c_str());
	return false;
}

namespace command {

// TODO usage
void
add(std::string name, std::function<void(int argc, std::vector<std::string> argv)> run)
{
	concmd_t concmd;
	concmd.name = name;
	concmd.min_args = 0;
	concmd.run = run;
	concmds.push_back(concmd);
}

void
add_action(std::string name, std::function<void(bool act)> run)
{
	conaction_t conact;
	conact.name = name;
	conact.run = run;
	conactions.push_back(conact);
}

}

} // namespace console
