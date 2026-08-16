CC ?= gcc
CFLAGS ?= -Wall -Wextra -Werror -D_FORTIFY_SOURCE=2 -O2
LDFLAGS ?=
LDLIBS ?=

TARGET = app
SRCS = cfetch.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
