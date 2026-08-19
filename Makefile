CC ?= gcc
CFLAGS ?= -Wall -Wextra -Werror -D_FORTIFY_SOURCE=2 -O2
CPPFLAGS ?= -I$(INCDIR)
LDFLAGS ?=
LDLIBS ?=

TARGET   = cfetch
SRCDIR   = src
INCDIR   = include
BUILDDIR = build

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
DEPS = $(OBJS:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR) $(TARGET)

-include $(DEPS)
