# Imprimis

*the destroyable world 3D shooter*

Imprimis is a multiplayer first person shooter, built on the Libprimis engine.
The game focuses on tactical gameplay enabled by the ability for players to change
the world they play in, whether to create fortifications to increase their defensive
ability or to destroy cover possessed by the enemy.

Imprimis is an open source game, built on an open source engine. All assets used
to create the game are likewise open sourced.

## Program Scope

The Imprimis repository contains the game implementation as well as the necessary
enet networking library, the headers required to build against the engine, and the
game assets (all located in submodules).

The Imprimis game requires a separate server in order to play locally. For the
game server, see https://github.com/project-imprimis/imprimis-gameserver.

## Linux Installation Instructions

Imprimis requires the `libprimis` shared library, which can be created by building
the libprimis engine located at https://github.com/project-imprimis/libprimis.

Imprimis requires the library built by libprimis (`libprimis.so`) to be located in
one of the standard Linux library directories (typically `/usr/lib/`). If the
`libprimis.so` file has not been places there, Imprimis will fail to run once it
has been compiled (as it cannot find the needed shared library).

Once libprimis is installed, Imprimis can be successfully installed. The project
can be downloaded by running the following command at the desired install location:
`git clone https://github.com/project-imprimis/imprimis.git --recurse-submodules`

Once the game has been downloaded, access the game directory with `cd imprimis`.
The game can now be compiled with `make` (or `make -jN` for N threaded compilation).

Imprimis can then be run by executing the `imprimis_unix` script, located in the
root Imprimis directory.
