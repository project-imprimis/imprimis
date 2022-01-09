# Turn warning on all constructions that are easily avoidable.
CXXFLAGS = -Wall
# Set the optmizization level to 3, the highest.
CXXFLAGS += -O3
# Increase speed but reduce precision. Precision is not needed.
CXXFLAGS += -ffast-math
# Makes characters signed.
CXXFLAGS += -fsigned-char
# Specify the target architecture. May use 'native' instead.
CXXFLAGS += -march=x86-64
# Needed to create a shared, dynamically linked library that cppyy can load.
CXXFLAGS += -shared -fpic
# Use C++ version 17.
CXXFLAGS += -std=c++17

# Set appropriate library includes.
CLIENT_INCLUDES= -Igame -Ienet/include -I/usr/X11R6/include `sdl2-config \
	--cflags`

# TODO, is -Lenet required?
CLIENT_LIBS= -lprimis -Lenet -lenet -L/usr/X11R6/lib -lX11 `sdl2-config --libs` \
	-lSDL2_image -lSDL2_mixer -lz -lGL

# List of source code files to be compiled.
CLIENT_OBJS= \
	game/ai.o \
	game/client.o \
	game/cserver.o \
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

# Define the library output name.
LIBRARY_NAME= imprimis.so

# Call `make` to make the client. Speed up compilation by increasing the number
# of threads. For example, `make -j2` or `make -j4`.
default: client

# Client needs libenet and client objects. CXX stands for C++ compiler, while
# CXXFLAGS stands for C++ compiler flags.
client:	libenet $(CLIENT_OBJS)
	 $(CXX) $(CXXFLAGS) -o $(LIBRARY_NAME) $(CLIENT_OBJS) $(CLIENT_LIBS)

# Add CLIENT_INCLUDES flags to C++ compiler.
$(CLIENT_OBJS): CXXFLAGS += $(CLIENT_INCLUDES)

# Libenet needs enet/libenet.a.
libenet: enet/libenet.a

# Make enet/libenet.a.
enet/libenet.a:
	$(MAKE) -C enet

# Call `make clean` to cleanup build files and executable.
clean:
	-$(RM) -r $(CLIENT_OBJS) $(LIBRARY_NAME)

# Call `remove-build-files` to remove all of the assets required to build, and
# just leave what is needed to run program.
remove-build-files:
	-$(RM) -r game/ vcpp/ bin64/ enet/ libprimis-headers/ .git/ .semaphore/ \
	imprimis.bat .gitmodules Makefile