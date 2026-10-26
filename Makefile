CC      ?= gcc
CFLAGS  ?= -O2
CFLAGS  += -std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -Werror
PREFIX  ?= /usr
DESTDIR ?=

BIN  = tr181ctl
SRCS = src/main.c src/datamodel.c
OBJS = $(SRCS:.c=.o)

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

src/%.o: src/%.c src/datamodel.h
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(BIN)
	sh tests/run.sh ./$(BIN)

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 0755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)

clean:
	rm -f $(BIN) $(OBJS)

.PHONY: all test install clean
