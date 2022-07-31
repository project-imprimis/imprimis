CXXFLAGS ?= -O3 -ffast-math -Wall
CXXFLAGS += -std=c++17 -march=x86-64 -fsigned-char

# install prefix, configurable by the user
PREFIX ?= /usr/local
# the DESTDIR variable is also used as an install prefix, but is meant to only be used by package builders for system images

#set appropriate library includes
CLIENT_INCLUDES= -Igame -Ienet/include -I/usr/X11R6/include `sdl2-config --cflags`
CLIENT_LIBS= -lprimis -Lenet -lenet -L. -L/usr/X11R6/lib -lX11 `sdl2-config --libs` -lSDL2_image -lSDL2_mixer -lz -lGL -lGLEW

#list of source code files to be compiled
CLIENT_OBJS= \
	game/ai.o \
	game/client.o \
	game/cserver.o \
	game/crypto.o \
	game/edit.o \
	game/entities.o \
	game/game.o \
	game/gameclient.o \
	game/main.o \
	game/minimap.o \
	game/render.o \
	game/scoreboard.o \
	game/server.o \
	game/serverbrowser.o \
	game/waypoint.o \
	game/waypointai.o \
	game/weapon.o

default: client

install: client emplace

#cleanup build files and executable
clean:
	-$(RM) -r $(CLIENT_OBJS) tess_client

#remove all of the assets required to build, just leaves what is needed to run program
remove-build-files:
	-$(RM) -r game/ vcpp/ bin64/ enet/ libprimis-headers/ .git/ .semaphore/ imprimis.bat .gitmodules Makefile

$(CLIENT_OBJS): CXXFLAGS += $(CLIENT_INCLUDES)

client:	libenet $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o native_client $(CLIENT_OBJS) $(CLIENT_LIBS)

enet/libenet.a:
	$(MAKE) -C enet
libenet: enet/libenet.a

emplace: uninstall  # clean out installation locations to prevent pollution
	# initialize installation directories (for minimal packager filesystems)
	mkdir --parents $(DESTDIR)$(PREFIX)/bin/
	mkdir --parents $(DESTDIR)$(PREFIX)/lib/
	mkdir --parents $(DESTDIR)$(PREFIX)/share/applications/
	mkdir --parents $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/
	mkdir --parents $(DESTDIR)$(PREFIX)/share/metainfo/
	cp -R ./ $(DESTDIR)$(PREFIX)/lib/imprimis
	# edit install dir, not source
	cd $(DESTDIR)$(PREFIX)/lib/imprimis; \
		rm -rf game/ vcpp/ bin64/ enet/ libprimis-headers/ .git/ .semaphore/ imprimis.bat .gitmodules Makefile; \
 		sed -i "s|=\.$$|=$(PREFIX)/lib/imprimis|" imprimis_unix; \
		mv ./imprimis_unix $(DESTDIR)$(PREFIX)/bin/imprimis; \
		mv ./media/interface/icon.svg $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/org.imprimis.Imprimis.svg; \
		mv ./org.imprimis.Imprimis.desktop $(DESTDIR)$(PREFIX)/share/applications/org.imprimis.Imprimis.desktop; \
		mv ./org.imprimis.Imprimis.metainfo.xml $(DESTDIR)$(PREFIX)/share/metainfo/org.imprimis.Imprimis.metainfo.xml;

uninstall:
	rm -rf $(DESTDIR)$(PREFIX)/lib/imprimis/ \
	       $(DESTDIR)$(PREFIX)/bin/imprimis \
	       $(DESTDIR)$(PREFIX)/share/icons/hicolor/scalable/apps/org.imprimis.Imprimis.svg \
	       $(DESTDIR)$(PREFIX)/share/applications/org.imprimis.Imprimis.desktop \
	       $(DESTDIR)$(PREFIX)/share/metainfo/org.imprimis.Imprimis.metainfo.xml

