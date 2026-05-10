#ifndef XPP_INPUT_HPP
#define XPP_INPUT_HPP

#include <string>
#include <SDL2/SDL.h>

namespace xpp::input {
	void		init(void);
	void		update(void);
	bool		bind(int key, std::string command);
	bool		bind(std::string key, std::string command);
	std::string	get_bind(int key);
	std::string	get_bind(std::string key);
	bool		unbind(int key);
	bool		unbind(std::string key);
	void		unbindall(void);
	void		setkey(int key, int value);
	int			getkeyoverride(int key);
	int			getkeyraw(int key);
	int			getkey(int key);
	int			getmousex(void);
	int			getmousey(void);
	int			getmousebutton(int button);
	bool		getjoy(int button);
	//bool		getjoypressed(int button);
	//bool		getjoyreleased(int button);
}

#endif