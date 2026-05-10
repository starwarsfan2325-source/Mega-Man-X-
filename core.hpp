#ifndef XPP_CORE_HPP
#define XPP_CORE_HPP

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#ifdef DEBUG
#define XPP_DEBUG " DEBUG"
#else
#define XPP_DEBUG " RELEASE"
#endif
#define XPP_VERSION "0.1.0" XPP_DEBUG

// TODO remove me for release build IMPORTANT FIXME
#ifdef DEBUG
#define SOL_ALL_SAFETIES_ON 1
#endif
//#include "sol/sol.hpp"

namespace sol {
	class state;
}

extern sol::state lua;

#define CONSOLE_WARN		1
#define CONSOLE_ERROR		2
#define CONSOLE_LINK		3

extern "C" {
void			panic(const char *fmt, ...);
void			printc(const char *fmt, ...);
size_t		printx(unsigned char level, const char *fmt, ...);
const char *	stringf(const char *fmt, ...);
void			perf_begin(void);
void			perf_end(void);
}

void HelpMarker(const char *desc);

#endif
