
CXXFLAGS += -Wall -Wpedantic

SRCS = decode_mat.c bitmap.c

OBJS = $(SRCS:.c=.o)

decode_mat: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(OBJS) decode_mat
