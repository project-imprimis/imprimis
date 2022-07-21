CXXFLAGS ?= -O3 -ffast-math -Wall
CXXFLAGS += -std=c++17 -march=x86-64 -fsigned-char

PREFIX ?= /usr/local

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
	mkdir --parents $(DESTDIR)$(PREFIX)/share/
	mkdir --parents $(DESTDIR)$(PREFIX)/share/pixmaps/
	mkdir --parents $(DESTDIR)$(PREFIX)/bin/
	cp -R ./ $(DESTDIR)$(PREFIX)/share/imprimis
	# edit install dir, not source
	cd $(DESTDIR)$(PREFIX)/share/imprimis; \
		rm -rf game/ vcpp/ bin64/ enet/ libprimis-headers/ .git/ .semaphore/ imprimis.bat .gitmodules Makefile; \
		# set launcher script to launch from installed binaries
		sed -i "s|=\.$$|=$(DESTDIR)$(PREFIX)/share/imprimis|" imprimis_unix; \
		mv ./imprimis_unix $(DESTDIR)$(PREFIX)/bin/imprimis; \
		cp ./media/interface/icon.png $(DESTDIR)$(PREFIX)/share/pixmaps/imprimis.png

uninstall:
	rm -rf $(DESTDIR)$(PREFIX)/share/imprimis
	rm -rf $(DESTDIR)$(PREFIX)/share/pixmaps/imprimis.png
	rm -rf $(DESTDIR)$(PREFIX)/bin/imprimis
