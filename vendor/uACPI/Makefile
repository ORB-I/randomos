CFLAGS := $(DFLCFLAGS) -Iinclude
LIB := ../../lib/libuacpi.a
SRCS := $(shell find . -name "*.c")
OBJS := $(SRCS:.c=.o)

$(LIB): $(OBJS)
	$(AR) rcs $@ $^
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f $(OBJS) $(LIB)