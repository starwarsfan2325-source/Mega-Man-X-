#ifndef XPP_SCRIPT_HPP
#define XPP_SCRIPT_HPP

#define SOL_EXCEPTIONS_SAFE_PROPAGATION 1
#include "core.hpp"
#include "sol/sol.hpp"
#include <unordered_map>

class TextEditor;

namespace xpp::script {
	struct script_t {
		TextEditor *editor;
		std::string path;
		std::string filename;
		std::string text;
		bool modified;
	};

	extern std::unordered_map<std::string, script_t> scripts; // TODO use std::map

	void init(void);
	void load_kernel(void);
	bool load(std::string filename, bool exec = true);
	bool load_dir(std::string dir = "lua", bool exec = true);
	bool run(std::string str, std::string filename = "");
	bool save(std::string s, std::string filename = "");
	bool check_result(sol::protected_function_result r, std::string filename = "");
	std::string to_string(sol::object o);
	std::string type_to_string(int t);
	std::string type_to_string(sol::type t);
	std::string parse_sol_filename(std::string str);
	int parse_sol_line_number(std::string str);
	int parse_sol_line_number_chunk(std::string str);
}

#endif