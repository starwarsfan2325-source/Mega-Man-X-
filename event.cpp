#include "event.hpp"

namespace event {

namespace {

std::map<std::string, std::vector<std::function<void()>>> subscribers_void;

} // namespace

// interface

void
subscribe(std::string name, std::function<void()> f)
{
	subscribers_void[name].push_back(std::move(f));
}

bool
fire(std::string name)
{
	bool handled = false;
	sol::function intercept = lua["event"]["intercept"][name.c_str()];
	if (intercept.valid()) {
		intercept(name);
		handled = true;
	} else {
		intercept = lua["event"]["intercept"]["default"];
		intercept(name);
	}
	for (auto &f : subscribers_void[name]) {
		f();
		handled = true;
	}
	return handled;
}

bool
fire_lua(std::string name)
{
	bool handled = false;
	sol::function intercept = lua["event"]["intercept"][name.c_str()];
	if (intercept.valid()) {
		intercept(name);
		handled = true;
	} else {
		intercept = lua["event"]["intercept"]["default"];
		intercept(name);
	}
	for (auto &f : subscribers_void[name]) {
		f();
		handled = true;
	}
	return handled;
}

} // namespace
