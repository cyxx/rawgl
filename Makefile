
SDL_CFLAGS = `sdl-config --cflags`
SDL_LIBS = `sdl-config --libs`

DEFINES = -DLITTLE_ENDIAN

CXX = g++
CXXFLAGS:= -g -O2 -Wall -Wuninitialized -Wno-unknown-pragmas -Wshadow -Wstrict-prototypes
CXXFLAGS+= -Wimplicit -Wundef -Wreorder -Wwrite-strings -Wnon-virtual-dtor -Wno-multichar
CXXFLAGS+= $(SDL_CFLAGS) $(DEFINES)

SRCS = bank.cpp file.cpp engine.cpp logic.cpp resource.cpp sdlstub.cpp \
	serializer.cpp staticres.cpp util.cpp video.cpp main.cpp

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

raw: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(SDL_LIBS) -lz

.cpp.o:
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $*.o

clean:
	rm -f *.o *.d

-include $(DEPS)
