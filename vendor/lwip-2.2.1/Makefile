CFLAGS := $(DFLCFLAGS) -Isrc/include -I../../include -w
LIB := ../../lib/liblwip.a
SRCS := $(shell find src/core -name '*.c') \
        $(shell find src/api -name '*.c') \
		$(shell find src/netif -name '*.c')
OBJS := $(SRCS:.c=.o)

$(LIB): $(OBJS)
	$(AR) rcs $@ $^
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f $(OBJS) $(LIB)