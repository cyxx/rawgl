
SDL_CFLAGS = `sdl-config --cflags`
SDL_LIBS = `sdl-config --libs` -lGL

DEFINES = -DBYPASS_PROTECTION

CXX = g++
CXXFLAGS := -g -O -Wall -Wuninitialized -Wno-unknown-pragmas -Wshadow
CXXFLAGS += -Wundef -Wreorder -Wwrite-strings -Wnon-virtual-dtor -Wno-multichar
CXXFLAGS += $(SDL_CFLAGS) $(DEFINES)

SRCS = file.cpp engine.cpp graphics.cpp script.cpp mixer.cpp pak.cpp resource.cpp systemstub_ogl.cpp \
	sfxplayer.cpp staticres.cpp unpack.cpp util.cpp video.cpp main.cpp

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

rawgl: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(SDL_LIBS)

clean:
	rm -f $(OBJS) $(DEPS)

-include $(DEPS)
