#include "core.hpp"
#include <stdlib.h>
#include <time.h>
#include "imgui/imgui.h"

static clock_t timer;

static char panicbuf[1024];

void __attribute__((noreturn))
panic(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vsnprintf(panicbuf, sizeof(panicbuf), fmt, va);
	va_end(va);

	fprintf(stderr, "\nEngine panic: %s\n", panicbuf);
	fflush(stdout);
	fflush(stderr);
	exit(EXIT_FAILURE);
}

static char stringfbuf[1024];

const char *
stringf(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	vsnprintf(stringfbuf, sizeof(stringfbuf), fmt, va);
	va_end(va);

	return stringfbuf;
}

void
perf_begin(void)
{
	timer = clock();
}

void
perf_end(void)
{
	timer = clock() - timer;
	printc("PERF: %.08f\n", ((double)timer)/CLOCKS_PER_SEC);
}

void
HelpMarker(const char *desc)
{
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}