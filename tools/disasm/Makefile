
CXXFLAGS:= -g -O -Wall -Wuninitialized -Wno-unknown-pragmas -Wshadow
CXXFLAGS+= -Wundef -Wreorder -Wwrite-strings -Wnon-virtual-dtor -Wno-multichar
CXXFLAGS+= -DSYS_LITTLE_ENDIAN

SRCS = disasm.cpp

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

disasm: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS)

.cpp.o:
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $*.o

clean:
	rm -f *.o *.d

-include $(DEPS)
