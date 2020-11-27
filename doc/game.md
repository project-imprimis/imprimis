# The Imprimis Game

#### This is a work in progress and subject to modification and additions.

Written and © Alex "no-lex" Foster, 2020; released under the WTFPL v2.

## Preface
---

This is the game structure and design document for Imprimis, built on the
libprimis game engine. This document does not explain how the engine
underpinning the engine functions and does not recap functionality contained
there; it is expected that the reader understand the engine functionality to
fully understand the technical design of the game.

This document also aims to provide the motavation and working principles of the
game itself, including the way the gameplay meta is constructed. The goal is to
make clear the design of the game such as to keep the game focused on a single
set of working principles (and to avoid confused, scatterbrained work on
conflicting design goals).

## Chapters

#### 1. Standards
* 1.1 Coding Standards
* 1.2 Default Paths & Libraries
* 1.3 Conventions and Units
* 1.4 Program Structure
* 1.5 System Contexts

#### 2. Game Design
* 2.1 Engine Considerations
* 2.2 Game Metaplay
* 2.3 Game Mechanics

#### 3. Game Implementation
* 3.1 Weapons
* 3.2 Game Variables
* 3.3 Modes
* 3.4 Entities

#### 4. Netcode
* 4.1 Topology
* 4.2 Server
* 4.3 Client
* 4.4 Master Server

# 4 Netcode
---

Like Tesseract and Cube 2, Imprimis runs a very client-heavy network topology
that minimizes the workload for the server itself. The server does little more
than redirect packets, while the individual clients have the burden of
interpreting what is passed to them by other clients.

As a result, Imprimis trades a good deal of security for a very simple and
responsive networking experience. It is not particularly difficult to create
clients which exploit this behavior, but server-mediated behavior is problematic
for a first person shooter where reaction time is paramount.

# 4.1 Topology

The general topology of networking and its relation to the state of game state
(variables and information) is outlined below.

```
+------------------------+------------------------+------------------------+
| Local Client           | Dedicated Server       | Remote Clients         |
+------------------------+------------------------+------------------------+
|                  Synchronous &            Synchronous &                  |
| +-------+         ratelimited              ratelimited         +-------+ |
| |       +-----------+  .  +------------------+  .  +-----------+ Non   | |
| | Local |  Client   |  .  |                  |  .  |  Client   | Local | |
| | State | Broadcast |---->|- - - - - - - - ->|---->|   Server  | Remote| |
| |       +-----------+  .  |                  |  .  +-----------+ State | |
| +-------+              .  |   Multicaster    |  .              +-------+ |
| |  Non  +-----------+  .  |                  |  .  +-----------+       | |
| | Local |  Client   |<----|<- - - - - - - - -|<----|  Client   | Local | |
| | State |   Server  |  .  |                  |  .  | Broadcast | Remote| |
| |       +-----------+  .  +------------------+  .  +-----------+ State | |
| +-------+              .                        .              +-------+ |
+------------------------+------------------------+------------------------+
```

As indicated in the diagram, the dedicated server does not control the game
state; it merely shuffles packets between clients which update their nonlocal
state. All dynamic and detministic objects in the game are assigned to a client
as part of their local state, which other clients ("remote clients") read and
copy to their nonlocal state information.

Clients are not charged with interpreting the behavior of game objects outside
of their local state: they accept the results that they recieve to their
nonlocal state information.

## 4.2 Server
---

The server in Imprimis is always present, even in singleplayer, and relays
information between clients. In singleplayer or in multiplayer games with bots,
these bots have full client status and their information is relayed to the
player via the server.

The server is very sparse in its function, and only a handful of system
resources are required to run one: Raspberry Pis are generally adequate for this
task.

A server instance does not check the contents of the packets which it sees and
essentially only radios what the clients tell it to other clients. A server's
control over the game is limited to its control over the gamemode and game end
time (ensuring that no one client can try to end the match at its whim) and
managing client bans and other holds.

### 4.2.1 Protocol
---

The server talks to clients via UDP using the ENet library. As the game uses
the UDP protocol, the job of making packets is left to the ENet library, which
allows for skipping the time consuming checks that TCP requires.

ENet is IP v4 only currently, and therefore the game cannot resolve IP v6
addresses.

### 4.2.2 Server State
---

The state of a server, or the contents which it "knows" at any given time, is
quite limited. Servers know the following:

* Time left in the match
* Addresses of connected clients
* Type of client (local, remote, bot)
* Master server listing status
* Time since master server listing confirmation
* Location and status of server entities (pickup items)

The server does not know where players are and does not keep track of projectile
locations.

### 4.2.3 Ratelimiting
---

To prevent server bandwith packet spam attacks, the server limits packets to one
every 7ms (143/s). This is somewhat lower than the maximum packet transmission
speed of clients, but is sufficiently higher than refresh rates and reaction
times of players that this is not a significant problem. By doing so, dedicated
servers cannot be innundated with a client sending many packets in very short
succession.

## 4.3 Client
---

Clients are vastly more fleshed out in the Imprimis multiplayer system. Clients
manage not only actors (players) but also their projectiles. In this way, all
deterministic dynamic gameplay events are controlled by clients. Clients have
their own "clients" and "servers" which refer to the side of the netcode that
sends events (client) and those that recieve the events (server).

### 4.3.1 Packets
---

Clients send messages to other clients (via the server) in packets with one of
over one hundred `types` which encode what kind of action the packet applies to.
Some message types are of a fixed length (e.g. cube modification), but others
are of variable length (e.g. chat messages).

### 4.3.2 Client Scope
---

The scope of an individual client is quite large, as the clients alone must bear
the full weight of all dynamic deterministic events in the game. This includes:

* Player movement
* Projectile movement
* Hit/Kill Determination
* Geometry and Entity Modification
* Global Variable Changes

This does not include non-deterministic or non-dynamic behavior, such as:

* Static entity rendering (e.g. particle entities)
* Aesthetic rendering (ragdolls, stains, projectiles)

#### Hit/Kill Determination

Clients tell others when they've hit somebody else, rather than the reverse
(clients telling others when they've been hit). As a result, hit confirmation
should always look "right" for clients: the body that confirms the hit is the
one who fired the projectile.

This is notable because network lag can cause a player's broadcasted position to
differ from the position that the client itself believes it is at. As a result
of this, clients dealing with a laggy client don't have to trust that client's
percieved position to record a hit.

## 4.4 Master Server
---

The master server is a seperate piece of software which can serve as a directory
for clients to find game servers with. The master server does not host game
servers itself; it is a service hosted by an organization (such as the official
Imprimis project) that can be used for clients to see where servers are located.

The server browser in the game gets a list of currently listed servers from a
master server (defaulting to the official one) and then presents them in the
server browser. Game servers periodically send a sync message to the master
server, allowing for the master server to automatically delist stale servers
that have not recently responded.

There is additional functionality in the master server to support centralized
authentication, but due to archtectural concerns this usage is depreciated.
