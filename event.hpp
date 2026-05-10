#ifndef XPP_EVENT_HPP
#define XPP_EVENT_HPP

#include "core.hpp"
#include "sol/sol.hpp"
//#include "script.hpp"
#include <vector>
#include <map>
#include <functional>

namespace event {
	//template<class ... Types> struct Tuple {};

	template<typename... T>
	std::map<std::string, std::vector<std::function<void(T...)>>> subscribers;

	template<typename T> void
	subscribe(std::string name, std::function<void(T)> f)
	{
		subscribers<T>[name].push_back(std::move(f));
	}

	void subscribe(std::string name, std::function<void()> f);

	template<typename... T> bool
	fire(std::string name, T... data)
	{
		bool handled = false;
		sol::function intercept = lua["event"]["intercept"][name.c_str()];
		if (intercept.valid()) {
			intercept(name, data...);
			handled = true;
		} else {
			intercept = lua["event"]["intercept"]["default"];
			intercept(name, data...);
		}
		for (auto &f : subscribers<T...>[name]) {
			f(data...);
			handled = true;
		}
		return handled;
	}

	bool fire(std::string name);
	bool fire_lua(std::string name); // redundant but sol can't resolve fire
}

#endif