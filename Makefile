
SDL_CFLAGS = `sdl-config --cflags`
SDL_LIBS = `sdl-config --libs` -lGL

DEFINES = -DBYPASS_PROTECTION

CXX = g++
CXXFLAGS := -g -O -MMD -Wuninitialized -Wundef -Wreorder $(SDL_CFLAGS) $(DEFINES)

SRCS = file.cpp engine.cpp graphics.cpp script.cpp mixer.cpp pak.cpp resource.cpp resource_nth.cpp systemstub_ogl.cpp \
	scaler.cpp sfxplayer.cpp staticres.cpp unpack.cpp util.cpp video.cpp main.cpp

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

rawgl: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(SDL_LIBS) -lz

clean:
	rm -f $(OBJS) $(DEPS)

-include $(DEPS)
