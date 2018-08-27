
CXXFLAGS += -g -O -Wall -Wpedantic

SRCS = disasm.cpp

OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

disasm: $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS)

.cpp.o:
	$(CXX) $(CXXFLAGS) -MMD -c $< -o $*.o

clean:
	rm -f $(OBJS) $(DEPS) disasm

-include $(DEPS)
