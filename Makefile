# BUILD SETTINGS ###############################################################

PLATFORM := ATARI
TYRIAN_DIR = $(gamesdir)/tyrian
WITH_NETWORK := false
WITH_SOUNDBLASTER := true


ifeq ($(target),68060)
ATARI_CPU       := 68060
ATARI_CPUFLAG   := -m68060
ATARI_BIN       := tyrian60.prg
else ifeq ($(target),68020)
ATARI_CPU       := 68020
ATARI_CPUFLAG   := -m68020-60
ATARI_BIN       := tyrian20.prg
else
ATARI_CPU       := 68000
ATARI_CPUFLAG   := -m68000 -msoft-float
ATARI_BIN       := tyrian00.prg
endif

ATARI_PREFIX := /opt/cross-mint/m68k-atari-mint
ATARI_CFLAGS := -DATARI_CPU=$(ATARI_CPU) $(ATARI_CPUFLAG) -I$(ATARI_PREFIX)/include -I$(ATARI_PREFIX)/include/SDL
ATARI_LDFLAGS := $(ATARI_CPUFLAG) -L$(ATARI_PREFIX)/lib -lsdl

################################################################################

# see https://www.gnu.org/prep/standards/html_node/Makefile-Conventions.html

SHELL = /bin/sh

CC = m68k-atari-mint-gcc
INSTALL ?= install
PKG_CONFIG ?= pkg-config

VCS_IDREV ?= (git describe --tags || git rev-parse --short HEAD)

INSTALL_PROGRAM ?= $(INSTALL)
INSTALL_DATA ?= $(INSTALL) -m 644

prefix ?= /usr/local
exec_prefix ?= $(prefix)

bindir ?= $(exec_prefix)/bin
datarootdir ?= $(prefix)/share
datadir ?= $(datarootdir)
docdir ?= $(datarootdir)/doc/opentyrian
mandir ?= $(datarootdir)/man
man6dir ?= $(mandir)/man6
man6ext ?= .6

# see http://www.pathname.com/fhs/pub/fhs-2.3.html

gamesdir ?= $(datadir)/games

###

TARGET := $(ATARI_BIN)

SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:src/%.c=obj/%.o)
DEPS := $(SRCS:src/%.c=obj/%.d)

###

ifeq ($(WITH_NETWORK), true)
    EXTRA_CPPFLAGS += -DWITH_NETWORK
endif

ifeq ($(WITH_SOUNDBLASTER), true)
    EXTRA_CPPFLAGS += -DWITH_SOUNDBLASTER
endif

OPENTYRIAN_VERSION := $(shell $(VCS_IDREV) 2>/dev/null && \
                              touch src/opentyrian_version.h)
ifneq ($(OPENTYRIAN_VERSION), )
    EXTRA_CPPFLAGS += -DOPENTYRIAN_VERSION='"$(OPENTYRIAN_VERSION)"'
endif

CPPFLAGS := -DNDEBUG
CFLAGS := -pedantic
CFLAGS += -MMD
CFLAGS += -Wall \
          -Wextra \
          -Wno-missing-field-initializers
CFLAGS += -O2
LDFLAGS := 
LDLIBS := 

ifeq ($(WITH_NETWORK), true)
#    SDL_CPPFLAGS := $(shell $(PKG_CONFIG) sdl SDL_net --cflags)
#    SDL_LDFLAGS := $(shell $(PKG_CONFIG) sdl SDL_net --libs-only-L --libs-only-other)
#    SDL_LDLIBS := $(shell $(PKG_CONFIG) sdl SDL_net --libs-only-l)
else
#    SDL_CPPFLAGS := $(shell $(PKG_CONFIG) sdl --cflags)
#    SDL_LDFLAGS := $(shell $(PKG_CONFIG) sdl --libs-only-L --libs-only-other)
#    SDL_LDLIBS := $(shell $(PKG_CONFIG) sdl --libs-only-l)

    SDL_LDLIBS := -lsdl -lgem

endif

ALL_CPPFLAGS = -DTARGET_$(PLATFORM) \
               -DTYRIAN_DIR='"$(TYRIAN_DIR)"' \
               $(EXTRA_CPPFLAGS) \
               $(SDL_CPPFLAGS) \
               $(CPPFLAGS) \
			   $(ATARI_CFLAGS)

ALL_CFLAGS = -std=iso9899:1999 \
             $(CFLAGS) \
			 $(ATARI_CFLAGS)

ALL_LDFLAGS = $(SDL_LDFLAGS) \
              $(LDFLAGS) \
			  $(ATARI_LDFLAGS)

ALL_LDLIBS = -lm \
             $(SDL_LDLIBS) \
             $(LDLIBS)

###

.PHONY : all
all : $(TARGET)

.PHONY : debug
debug : CPPFLAGS += -UNDEBUG
debug : CFLAGS += -Werror
debug : CFLAGS += -O0
debug : CFLAGS += -g3
debug : all

.PHONY : installdirs
installdirs :
	mkdir -p $(DESTDIR)$(bindir)
	mkdir -p $(DESTDIR)$(docdir)
	mkdir -p $(DESTDIR)$(man6dir)

.PHONY : install
install : $(TARGET) installdirs
	$(INSTALL_PROGRAM) $(TARGET) $(DESTDIR)$(bindir)/
	$(INSTALL_DATA) CREDITS NEWS README $(DESTDIR)$(docdir)/
	$(INSTALL_DATA) linux/man/opentyrian.6 $(DESTDIR)$(man6dir)/opentyrian$(man6ext)

.PHONY : uninstall
uninstall :
	rm -f $(DESTDIR)$(bindir)/$(TARGET)
	rm -f $(DESTDIR)$(docdir)/{CREDITS,NEWS,README}
	rm -f $(DESTDIR)$(man6dir)/opentyrian$(man6ext)

.PHONY : clean
clean :
	rm -f $(OBJS)
	rm -f $(DEPS)
	rm -f $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(ALL_CFLAGS) $(ALL_LDFLAGS) -Wl,-Map mapfile -o $@ $^ $(ALL_LDLIBS)
	m68k-atari-mint-strip $(TARGET)
	m68k-atari-mint-flags -S $(TARGET)
	m68k-atari-mint-stack --fix=256k $(TARGET)

-include $(DEPS)

obj/%.o : src/%.c
	@mkdir -p "$(dir $@)"
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<
