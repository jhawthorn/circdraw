
SRCFILES := $(wildcard src/*.cc)
OBJFILES := $(SRCFILES:%.cc=%.o)
DEPFILES := $(OBJFILES:%.o=%.d)
CLEANFILES := $(CLEANFILES) $(DEPFILES) $(OBJFILES) circdraw
CXXFLAGS ?= -O3 -g $(shell sdl-config --cflags) $(shell pkg-config opencv --cflags) -Wall
OPENCV_LIBS := $(shell pkg-config opencv --libs)
SDL_LIBS := $(shell sdl-config --libs) -lSDL_image -lSDL_gfx
LIBS ?= $(SDL_LIBS) $(OPENCV_LIBS)
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

# User configuration
-include config.mk

circdraw: $(OBJFILES)
	$(CXX) $(LDFLAGS) -o $@ $(OBJFILES) $(LIBS)

-include $(DEPFILES)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -MMD -MP -MT "$*.d" -c -o $@ $<

install:
	install -Dm 755 meh $(BINDIR)

# Clean
clean:
	$(RM) $(CLEANFILES)

.PHONY: clean

