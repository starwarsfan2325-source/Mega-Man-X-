NPROC=2
CXXFLAGS=-DDEBUG -Wno-multichar -g -c -IGL -I/usr/include/luajit-2.0 -I./include -I./include/bgfx -I./include/bgfx/include -isystem./include/bgfx/3rdparty -I./bsnes -fpermissive -std=c++17 -Og -march=native
CFLAGS=-DDEBUG -Wno-multichar -g -c -I/usr/include/luajit-2.0
LDFLAGS=-g\
	-L/usr/local/lib\
	-L/h/src/external/bgfx/.build/linux64_gcc/bin\
	-lm\
	-ldl\
	-lpthread\
	-lGL\
	-lX11\
	-lluajit-5.1\
	-lbgfxDebug\
	-lbimgDebug\
	-lbxDebug\
	#-lbimg_decodeDebug\
	-lSDL2\
	-lasound\
	-lbsnes\

OFILES=\
	#gl3w.o\
	bin/main.o\
	bin/core.o\
	bin/video.o\
	bin/renderer.o\
	bin/tilemap.o\
	bin/console.o\
	bin/input.o\
	bin/sound.o\
	bin/animation.o\
	bin/script.o\
	bin/event.o\
	bin/editor.o\
	bin/emulator.o\
	bin/xgui/xgui.o\
	bin/xgui/about_w.o\
	bin/xgui/console_w.o\
	bin/xgui/demo_w.o\
	bin/xgui/docs_w.o\
	bin/xgui/editor_w.o\
	bin/xgui/lua_w.o\
	bin/xgui/input_w.o\
	bin/xgui/graphics_w.o\
	bin/xgui/state_w.o\
	bin/xgui/memory_w.o\
	bin/xgui/file_select_w.o\
	bin/xgui/TextEditor.o\
	bin/imgui/imgui.o\
	bin/imgui/imgui_demo.o\
	bin/imgui/imgui_draw.o\
	bin/imgui/imgui_widgets.o\
	bin/imgui/examples/imgui_impl_sdl.o\
	#bin/imgui/examples/imgui_impl_opengl2.o\
	bin/include/bgfx/examples/common/imgui/imgui.o\

#--retain-symbols-file bin/symbols.txt
#-rdynamic lets LuaJIT see executable's symbols

bin/xpp: $OFILES
	c++ -fPIC -rdynamic $OFILES $LDFLAGS -o bin/xpp

bin/%.o: %.cpp
	c++ -fPIC $CXXFLAGS $stem.cpp -o $target

bin/%.o: %.c
	cc -fPIC $CFLAGS $stem.c -o $target

clean:V:
	rm -f bin/*.o bin/xgui/*.o bin/xpp

cleangui:V:
	rm -f bin/xgui/*.o bin/xpp

run:V:
	cd bin && catchsegv ./xpp

debug:V:
	cd bin && gdb ./xpp
