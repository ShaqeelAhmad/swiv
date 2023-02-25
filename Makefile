WL_SCANNER = wayland-scanner
WL_PROTOCOLS_DIR = /usr/share/wayland-protocols/
XDG_SHELL = $(WL_PROTOCOLS_DIR)/stable/xdg-shell/xdg-shell.xml
XDG_DECORATION = $(WL_PROTOCOLS_DIR)/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml

WL_SRC = xdg-shell-protocol.c xdg-decoration-unstable-protocol.c
WL_HDR = xdg-shell-client-protocol.h xdg-decoration-unstable-client-protocol.h

version = 0.8

srcdir = .
VPATH = $(srcdir)

PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

# autoreload backend: inotify/nop
AUTORELOAD = inotify

# enable features requiring giflib (-lgif)
HAVE_GIFLIB = 1

# enable features requiring libexif (-lexif)
HAVE_LIBEXIF = 1

cflags = -std=c99 -Wall -pedantic $(CFLAGS)

cppflags = -I. $(CPPFLAGS) -D_XOPEN_SOURCE=700 \
  -DHAVE_GIFLIB=$(HAVE_GIFLIB) -DHAVE_LIBEXIF=$(HAVE_LIBEXIF) \
		 -DX_DISPLAY_MISSING `pkg-config --cflags cairo pango`

lib_exif_0 =
lib_exif_1 = -lexif
lib_gif_0 =
lib_gif_1 = -lgif
ldlibs = $(LDLIBS) -lm -lImlib2 \
  $(lib_exif_$(HAVE_LIBEXIF)) $(lib_gif_$(HAVE_GIFLIB)) \
  `pkg-config --libs cairo pangocairo pango xkbcommon wayland-client wayland-cursor`

objs = autoreload_$(AUTORELOAD).o commands.o image.o main.o options.o \
  thumbs.o util.o window.o xdg-shell-protocol.o shm.o xdg-decoration-unstable-protocol.o

all: swiv

.PHONY: all clean install uninstall
.SUFFIXES:
.SUFFIXES: .c .o
$(V).SILENT:

swiv: $(objs)
	@echo "LINK $@"
	$(CC) $(LDFLAGS) -o $@ $(objs) $(ldlibs)

$(objs): Makefile swiv.h commands.lst config.h $(WL_HDR) $(WL_SRC)
options.o: version.h

.c.o:
	@echo "CC $@"
	$(CC) $(cflags) $(cppflags) -c -o $@ $<

xdg-shell-protocol.c:
	@echo "GEN $@"
	$(WL_SCANNER) private-code $(XDG_SHELL) $@

xdg-shell-client-protocol.h:
	@echo "GEN $@"
	$(WL_SCANNER) client-header $(XDG_SHELL) $@

xdg-decoration-unstable-protocol.c:
	@echo "GEN $@"
	$(WL_SCANNER) private-code $(XDG_DECORATION) $@

xdg-decoration-unstable-client-protocol.h:
	@echo "GEN $@"
	$(WL_SCANNER) client-header $(XDG_DECORATION) $@

config.h:
	@echo "GEN $@"
	cp $(srcdir)/config.def.h $@

version.h: Makefile .git/index
	@echo "GEN $@"
	v="$$(cd $(srcdir); git describe 2>/dev/null)"; \
	echo "#define VERSION \"$${v:-$(version)}\"" >$@

.git/index:

clean:
	rm -f *.o swiv
	rm -f $(WL_SRC)
	rm -f $(WL_HDR)

install: all
	@echo "INSTALL $(DESTDIR)$(PREFIX)/bin/swiv"
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp swiv $(DESTDIR)$(PREFIX)/bin/
	chmod 755 $(DESTDIR)$(PREFIX)/bin/swiv
	@echo "INSTALL $(DESTDIR)$(PREFIX)/man1/swiv.1"
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s!PREFIX!$(PREFIX)!g; s!VERSION!$(version)!g" swiv.1 \
		>$(DESTDIR)$(MANPREFIX)/man1/swiv.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/swiv.1
	@echo "INSTALL $(DESTDIR)$(PREFIX)/share/swiv/exec"
	mkdir -p $(DESTDIR)$(PREFIX)/share/swiv/exec
	cp exec/* $(DESTDIR)$(PREFIX)/share/swiv/exec/
	chmod 755 $(DESTDIR)$(PREFIX)/share/swiv/exec/*
	@echo "INSTALL $(DESTDIR)$(PREFIX)/share/applications/swiv.desktop"
	mkdir -p $(DESTDIR)$(PREFIX)/share/applications/
	cp swiv.desktop $(DESTDIR)$(PREFIX)/share/applications/swiv.desktop

uninstall:
	@echo "REMOVE $(DESTDIR)$(PREFIX)/bin/swiv"
	rm -f $(DESTDIR)$(PREFIX)/bin/swiv
	@echo "REMOVE $(DESTDIR)$(PREFIX)/man1/swiv.1"
	rm -f $(DESTDIR)$(MANPREFIX)/man1/swiv.1
	@echo "REMOVE $(DESTDIR)$(PREFIX)/share/swiv/exec"
	rm -rf $(DESTDIR)$(PREFIX)/share/swiv
	@echo "REMOVE $(DESTDIR)$(PREFIX)/share/applications/swiv.desktop"
	rm -f $(DESTDIR)$(PREFIX)/share/applications/swiv.desktop
