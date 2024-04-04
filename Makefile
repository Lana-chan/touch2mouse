CC = gcc

SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)
BINARY = touch2mouse

DEBUG = no
PROFILE = no
OPTIMIZATION = -O3

ifeq ($(DEBUG), yes)
	CFLAGS += -g
	OPTIMIZATION = -O0
endif

ifeq ($(PROFILE), yes)
	CFLAGS += -pg
endif

CFLAGS += $(OPTIMIZATION)

all: project

install: project
	cp $(BINARY) /usr/bin/

uninstall:
	rm /usr/bin/$(BINARY)

project: $(OBJECTS)
	$(CC) $(OBJECTS) $(CFLAGS) -o $(BINARY)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf *.o $(BINARY)

rebuild: clean all

.PHONY : clean
.SILENT : clean