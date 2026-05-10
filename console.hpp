#ifndef XPP_CONSOLE_HPP
#define XPP_CONSOLE_HPP

#include <stddef.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iterator>
#include <functional>

#define MIN_ARGC(n) {if ((argc - 1) < n) {printc("Args < %d", n); return;}}

extern "C" void printc(const char *fmt, ...);
extern "C" size_t printx(unsigned char level, const char *fmt, ...);

namespace xpp::console {

struct line_t {
	std::string string;
	int number;
	unsigned char level;
};

struct concmd_t {
	std::string name;
	std::function<void(int argc, std::vector<std::string> argv)> run;
	int min_args;
};

struct conaction_t {
	std::string name;
	std::function<void(bool act)> run;
};

extern std::vector<concmd_t>	concmds;
extern std::vector<line_t>		lines;

void	init(void);
void	loadcfg(std::string filename);
void	clear(void);
bool	send(std::string str, char prompt = '$');

namespace command {
void	add(std::string name, std::function<void(int argc, std::vector<std::string> argv)> run);
void	add_action(std::string name, std::function<void(bool act)> run);
}

}

#endif