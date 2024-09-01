#ifndef GAME_H_
#define GAME_H_
#define SDL_MAIN_HANDLED

#include "../libprimis-headers/cube.h"
#include "../libprimis-headers/iengine.h"
#include "../libprimis-headers/consts.h"
#include "../libprimis-headers/sound.h"

#include <list>

#include <enet/enet.h>

#include "nettools.h"

constexpr float stairheight = 4.1f; //max height in cubits of an allowable step (4 = 0.5m)
constexpr float floorz = 0.867f; //to be considered a level floor, slope is below this
constexpr float slopez = 0.5f; //maximum climbable slope
constexpr float wallz = 0.2f; //steeper than this is considered a wall
constexpr float gravity = 100.0f; //downwards force scale

extern bool hmapsel;

//command macros only used in the game--the engine does not use these anywhere
//create lambda and use as function pointer, then dereference it to cast as identfun
//unary + operator on lambda used to degrade it to a function pointer
#define ICOMMANDKNS(name, type, cmdname, nargs, proto, b) \
    bool cmdname = addcommand(name, reinterpret_cast<identfun>(+[] proto { b; }), nargs, type); \

#define ICOMMANDKN(name, type, cmdname, nargs, proto, b) ICOMMANDKNS(#name, type, cmdname, nargs, proto, b)
#define ICOMMANDK(name, type, nargs, proto, b) ICOMMANDKN(name, type,_icmd_##name, nargs, proto, b)
#define ICOMMANDNS(name, cmdname, nargs, proto, b) ICOMMANDKNS(name, Id_Command, cmdname, nargs, proto, b)
#define ICOMMANDN(name, cmdname, nargs, proto, b) ICOMMANDNS(#name, cmdname, nargs, proto, b)

/**
 * @def ICOMMAND(name, nargs, proto, b)
 *
 * @brief Creates an inline command.
 *
 * Creates an inline command which is assigned to the global identmap hash table.
 *
 * @param name the name of the command to make
 * @param nargs the cubescript paramater signature
 * @param proto the C++ function call signature
 * @param b the body of the function to call
 *
 */
#define ICOMMAND(name, nargs, proto, b) ICOMMANDN(name, _icmd_##name, nargs, proto, b)

//command macros
#define COMMANDKN(name, type, fun, nargs) static bool dummy_##fun = addcommand(#name, (identfun)fun, nargs, type)
#define COMMANDK(name, type, nargs) COMMANDKN(name, type, name, nargs)
#define COMMANDN(name, fun, nargs) COMMANDKN(name, Id_Command, fun, nargs)
#define COMMAND(name, nargs) COMMANDN(name, name, nargs)

#define LOOP_START(id, stack) if((id)->type != Id_Alias) return; identstack stack;

struct VSlot;

//defines game statics, like animation names, weapon variables, entity properties
//includes:
//animation names
//console message types
//weapon vars
//game state information
//game entity definition

enum
{
    ClientState_Alive = 0,
    ClientState_Dead,
    ClientState_Spawning,
    ClientState_Lagged,
    ClientState_Editing,
    ClientState_Spectator,
};

// animations
// used in render.cpp
enum
{
    Anim_Dead = Anim_GameSpecific, //1
    Anim_Dying,
    Anim_Idle,
    Anim_RunN,
    Anim_RunNE,
    Anim_RunE,
    Anim_RunSE,
    Anim_RunS,
    Anim_RunSW,
    Anim_RunW, //10
    Anim_RunNW,
    Anim_Jump,
    Anim_JumpN,
    Anim_JumpNE,
    Anim_JumpE,
    Anim_JumpSE,
    Anim_JumpS,
    Anim_JumpSW,
    Anim_JumpW,
    Anim_JumpNW, //20
    Anim_Sink,
    Anim_Swim,
    Anim_Crouch,
    Anim_CrouchN,
    Anim_CrouchNE,//unused
    Anim_CrouchE,//unused
    Anim_CrouchSE,//unused
    Anim_CrouchS,//unused
    Anim_CrouchSW,//unused
    Anim_CrouchW, //30 (unused)
    Anim_CrouchNW,//unused
    Anim_CrouchJump,
    Anim_CrouchJumpN,
    Anim_CrouchJumpNE,//unused
    Anim_CrouchJumpE,//unused
    Anim_CrouchJumpSE,//unused
    Anim_CrouchJumpS,//unused
    Anim_CrouchJumpSW,//unused
    Anim_CrouchJumpW,//unused
    Anim_CrouchJumpNW, //40 (unused)
    Anim_CrouchSink,
    Anim_CrouchSwim,
    Anim_Shoot,
    Anim_Pain,
    Anim_Edit,
    Anim_Lag,
    Anim_Win,
    Anim_Lose, //50
    Anim_GunIdle,
    Anim_GunShoot,
    Anim_VWepIdle,
    Anim_VWepShoot,
    Anim_NumAnims //57
};

// console message types

enum
{
    ConsoleMsg_Chat       = 1<<8,
    ConsoleMsg_TeamChat   = 1<<9,
    ConsoleMsg_GameInfo   = 1<<10,
    ConsoleMsg_FragSelf   = 1<<11,
    ConsoleMsg_FragOther  = 1<<12,
    ConsoleMsg_TeamKill   = 1<<13
};

// network quantization scale
const float DMF   = 16.0f,            // for world locations
            DNF   = 100.0f,           // for normalized vectors
            DVELF = 1.0f;              // for playerspeed based velocity vectors

//these are called "GamecodeEnt" to avoid name collision (as opposed to gameents or engine static ents)
enum                            // static entity types
{
    GamecodeEnt_NotUsed              = EngineEnt_Empty,        // entity slot not in use in map
    GamecodeEnt_Light                = EngineEnt_Light,        // lightsource, attr1 = radius, attr2 = red, attr3 = green, attr4 = blue, attr5 = flags
    GamecodeEnt_Mapmodel             = EngineEnt_Mapmodel,     // attr1 = index, attr2 = yaw, attr3 = pitch, attr4 = roll, attr5 = scale
    GamecodeEnt_Playerstart,                                   // attr1 = angle, attr2 = team
    GamecodeEnt_Particles            = EngineEnt_Particles,    // attr1 = index, attrs2-5 vary on particle index
    GamecodeEnt_MapSound             = EngineEnt_Sound,        // attr1 = index, attr2 = sound
    GamecodeEnt_Spotlight            = EngineEnt_Spotlight,    // attr1 = angle
    GamecodeEnt_Decal                = EngineEnt_Decal,        // attr1 = index, attr2 = yaw, attr3 = pitch, attr4 = roll, attr5 = scale
    GamecodeEnt_MaxEntTypes,                                   // used for looping through full enum
};

enum
{
    Gun_Rail = 0,
    Gun_Pulse,
    Gun_Eng,
    Gun_Shotgun,
    Gun_Carbine,
    Gun_NumGuns
};
enum
{
    Act_Idle = 0,
    Act_Shoot,
    Act_NumActs
};

enum
{
    Attack_RailShot = 0,
    Attack_PulseShoot,
    Attack_EngShoot,
    Attack_ShotgunShoot,
    Attack_CarbineShoot,
    Attack_NumAttacks
};

inline bool validgun(int n)
{
    return (n) >= 0 && (n) < Gun_NumGuns;
}
inline bool validattack(int n)
{
    return (n) >= 0 && (n) < Attack_NumAttacks;
}

//enum of gameplay mechanic flags; bitwise sum determines what a mode's attributes are
enum
{
    Mode_Team           = 1<<0,
    Mode_CTF            = 1<<1,
    Mode_AllowOvertime  = 1<<2,
    Mode_Edit           = 1<<3,
    Mode_Demo           = 1<<4,
    Mode_LocalOnly      = 1<<5,
    Mode_Lobby          = 1<<6,
    Mode_Rail           = 1<<7,
    Mode_Pulse          = 1<<8,
    Mode_All            = 1<<9
};

enum
{
    Mode_Untimed         = Mode_Edit|Mode_LocalOnly|Mode_Demo,
    Mode_Bot             = Mode_LocalOnly|Mode_Demo
};

const struct gamemodeinfo
{
    const char *name, *prettyname;
    int flags;
    const char *info;
} gamemodes[] =
//list of valid game modes with their name/prettyname/game flags/desc
{
    { "demo", "Demo", Mode_Demo | Mode_LocalOnly, nullptr},
    { "edit", "Edit", Mode_Edit | Mode_All, "Cooperative Editing:\nEdit maps with multiple players simultaneously." },
    { "tdm", "TDM", Mode_Team | Mode_All, "Team Deathmatch: fight for control over the map" },
};

//these are the checks for particular mechanics in particular modes
//e.g. MODE_RAIL sees if the mode only have railguns
const int startgamemode = -1;

const int numgamemodes = static_cast<int>(sizeof(gamemodes)/sizeof(gamemodes[0]));

//check fxn
inline bool modecheck(int mode, int flag)
{
    if((mode) >= startgamemode && (mode) < startgamemode + numgamemodes) //make sure input is within valid range
    {
        if(gamemodes[(mode) - startgamemode].flags&(flag))
        {
            return true;
        }
        return false;
    }
    return false;
}

inline bool validmode(int mode)
{
    return (mode) >= startgamemode && (mode) < startgamemode + numgamemodes;
}

enum
{
    MasterMode_Auth = -1,
    MasterMode_Open = 0,
    MasterMode_Veto,
    MasterMode_Locked,
    MasterMode_Private,
    MasterMode_Password,
    MasterMode_Start = MasterMode_Auth,
    MasterMode_Invalid = MasterMode_Start - 1
};

struct servinfo
{
    string name, map, desc;
    int protocol, numplayers, maxplayers, ping;
    std::vector<int> attr;

    servinfo() : protocol(INT_MIN), numplayers(0), maxplayers(0)
    {
        name[0] = map[0] = desc[0] = '\0';
    }
};

const char * const mastermodenames[] =  { "auth",   "open",   "veto",       "locked",     "private",    "password" };
const char * const mastermodecolors[] = { "",       "^f0",    "^f2",        "^f2",        "^f3",        "^f3" };
const char * const mastermodeicons[] =  { "server", "server", "serverlock", "serverlock", "serverpriv", "serverpriv" };

// crypto

/**
 * @brief Creates a private-public key pair from a given seed value.
 *
 * The function takes a reference to the desired private and public keys, and
 * then modifies the contents of both strings using the seed. The modified
 * strings are the matching private-public key pair.
 * @param seed The seed value for generating random private-public key pairs.
 * @param privstr The private string to modify.
 * @param pubstr The public string to modify.
 */
extern void genprivkey(const char *seed, std::vector<char> &privstr, std::vector<char> &pubstr);

/**
 * @brief Verify a public key against a private key.
 *
 * Verify that the given public key and the given private key make a matching pair.
 * @param privstr The private key that was generated as part of a private-public key pair.
 * @param pubstr The public key that was generated as part of a private-public key pair.
 * @return true If the strings match and make a private-public key pair.
 * @return false If the strings do not match and do not make a private-public key pair.
 */
extern bool calcpubkey(const char *privstr, std::vector<char> &pubstr);
extern bool hashstring(const char *str, char *result, int maxlen);
extern void answerchallenge(const char *privstr, const char *challenge, std::vector<char> &answerstr);


// hardcoded sounds, defined in sounds.cfg
enum
{
    Sound_Jump = 0,
    Sound_Land,
    Sound_SplashIn,
    Sound_SplashOut,
    Sound_Burn,
    Sound_Melee,
    Sound_Pulse1,
    Sound_Pulse2,
    Sound_PulseExplode,
    Sound_Rail1,
    Sound_Rail2,
    Sound_WeapLoad,
    Sound_Hit,
    Sound_Die1,
    Sound_Die2,
    Sound_Carbine1,
    Sound_Shotgun1
};

// network messages codes, c2s, c2c, s2c

enum
{
    Discon_None = 0,
    Discon_EndOfPacket,
    Discon_Local,
    Discon_Kick,
    Discon_MsgError,
    Discon_IPBan,
    Discon_Private,
    Discon_MaxClients,
    Discon_Timeout,
    Discon_Overflow,
    Discon_Password,
    Discon_NumDiscons
};

enum
{
    Priv_None = 0,
    Priv_Master,
    Priv_Auth,
    Priv_Admin
};

enum
{
    NetMsg_Connect = 0,
    NetMsg_ServerInfo,
    NetMsg_Welcome,
    NetMsg_InitClient,
    NetMsg_Pos,
    NetMsg_Text,
    NetMsg_Sound,
    NetMsg_ClientDiscon,
    NetMsg_Shoot,
    //game
    NetMsg_Explode,
    NetMsg_Suicide, //10
    NetMsg_Died,
    NetMsg_Damage,
    NetMsg_Hitpush,
    NetMsg_ShotFX,
    NetMsg_ExplodeFX,
    NetMsg_TrySpawn,
    NetMsg_SpawnState,
    NetMsg_Spawn,
    NetMsg_ForceDeath,
    NetMsg_GunSelect, //20
    NetMsg_MapChange,
    NetMsg_MapVote,
    NetMsg_TeamInfo,
    NetMsg_ItemSpawn,
    NetMsg_ItemPickup,
    NetMsg_ItemAcceptance,

    NetMsg_Ping,
    NetMsg_Pong,
    NetMsg_ClientPing,
    NetMsg_TimeUp, //30
    NetMsg_ForceIntermission,
    NetMsg_ServerMsg,
    NetMsg_ItemList,
    NetMsg_Resume,
    //edit
    NetMsg_EditMode,
    NetMsg_EditEnt,
    NetMsg_EditFace,
    NetMsg_EditTex,
    NetMsg_EditMat,
    NetMsg_EditFlip, //40
    NetMsg_Copy,
    NetMsg_Paste,
    NetMsg_Rotate,
    NetMsg_Replace,
    NetMsg_DelCube,
    NetMsg_AddCube,
    NetMsg_CalcLight,
    NetMsg_Remip,
    NetMsg_EditVSlot,
    NetMsg_Undo,
    NetMsg_Redo, //50
    NetMsg_Newmap,
    NetMsg_GetMap,
    NetMsg_SendMap,
    NetMsg_Clipboard,
    NetMsg_EditVar,
    //master
    NetMsg_MasterMode,
    NetMsg_Kick,
    NetMsg_ClearBans,
    NetMsg_CurrentMaster,
    NetMsg_Spectator, //60
    NetMsg_SetMaster,
    NetMsg_SetTeam,
    //demo
    NetMsg_ListDemos,
    NetMsg_SendDemoList,
    NetMsg_GetDemo,
    NetMsg_SendDemo,
    NetMsg_DemoPlayback,
    NetMsg_RecordDemo,
    NetMsg_StopDemo,
    NetMsg_ClearDemos, //70
    //misc
    NetMsg_SayTeam,
    NetMsg_Client,
    NetMsg_AuthTry,
    NetMsg_AuthKick,
    NetMsg_AuthChallenge,
    NetMsg_AuthAnswer,
    NetMsg_ReqAuth,
    NetMsg_PauseGame,
    NetMsg_GameSpeed,
    NetMsg_AddBot, //80
    NetMsg_DelBot,
    NetMsg_InitAI,
    NetMsg_FromAI,
    NetMsg_BotLimit,
    NetMsg_BotBalance,
    NetMsg_MapCRC,
    NetMsg_CheckMaps,
    NetMsg_SwitchName,
    NetMsg_SwitchModel,
    NetMsg_SwitchColor, //90
    NetMsg_SwitchTeam,
    NetMsg_ServerCommand,
    NetMsg_DemoPacket,
    NetMsg_GetScore,
    NetMsg_GetRoundTimer,

    NetMsg_NumMsgs //95
};

/* list of messages with their sizes in bytes
 * the packet size needs to be known for the packet parser to establish how much
 * information to attribute to each message
 * zero indicates an unchecked or variable-size message
 */
const int msgsizes[] =
{
    NetMsg_Connect,       0,
    NetMsg_ServerInfo,    0,
    NetMsg_Welcome,       1,
    NetMsg_InitClient,    0,
    NetMsg_Pos,           0,
    NetMsg_Text,          0,
    NetMsg_Sound,         2,
    NetMsg_ClientDiscon,  2,

    NetMsg_Shoot,          0,
    NetMsg_Explode,        0,
    NetMsg_Suicide,        1,
    NetMsg_Died,           5,
    NetMsg_Damage,         5,
    NetMsg_Hitpush,        7,
    NetMsg_ShotFX,        10,
    NetMsg_ExplodeFX,      4,
    NetMsg_TrySpawn,       1,
    NetMsg_SpawnState,     8,
    NetMsg_Spawn,          4,
    NetMsg_ForceDeath,     2,
    NetMsg_GunSelect,      2,
    NetMsg_MapChange,      0,
    NetMsg_MapVote,        0,
    NetMsg_TeamInfo,       0,
    NetMsg_ItemSpawn,      2,
    NetMsg_ItemPickup,     2,
    NetMsg_ItemAcceptance, 3,

    NetMsg_Ping,          2,
    NetMsg_Pong,          2,
    NetMsg_ClientPing,    2,
    NetMsg_TimeUp,        2,
    NetMsg_ForceIntermission, 1,
    NetMsg_ServerMsg,     0,
    NetMsg_ItemList,      0,
    NetMsg_Resume,        0,

    NetMsg_EditMode,      2,
    NetMsg_EditEnt,      11,
    NetMsg_EditFace,     16,
    NetMsg_EditTex,      16,
    NetMsg_EditMat,      16,
    NetMsg_EditFlip,     14,
    NetMsg_Copy,         14,
    NetMsg_Paste,        14,
    NetMsg_Rotate,       15,
    NetMsg_Replace,      17,
    NetMsg_DelCube,      15,
    NetMsg_AddCube,      15,
    NetMsg_CalcLight,     1,
    NetMsg_Remip,         1,
    NetMsg_EditVSlot,    16,
    NetMsg_Undo,          0,
    NetMsg_Redo,          0,
    NetMsg_Newmap,        2,
    NetMsg_GetMap,        1,
    NetMsg_SendMap,       0,
    NetMsg_EditVar,       0,
    NetMsg_MasterMode,    2,
    NetMsg_Kick,          0,
    NetMsg_ClearBans,     1,
    NetMsg_CurrentMaster, 0,
    NetMsg_Spectator,     3,
    NetMsg_SetMaster,     0,
    NetMsg_SetTeam,       0,

    NetMsg_ListDemos,     1,
    NetMsg_SendDemoList,  0,
    NetMsg_GetDemo,       2,
    NetMsg_SendDemo,      0,
    NetMsg_DemoPlayback,  3,
    NetMsg_RecordDemo,    2,
    NetMsg_StopDemo,      1,
    NetMsg_ClearDemos,    2,

    NetMsg_SayTeam,       0,
    NetMsg_Client,        0,
    NetMsg_AuthTry,       0,
    NetMsg_AuthKick,      0,
    NetMsg_AuthChallenge, 0,
    NetMsg_AuthAnswer,    0,
    NetMsg_ReqAuth,       0,
    NetMsg_PauseGame,     0,
    NetMsg_GameSpeed,     0,
    NetMsg_AddBot,        2,
    NetMsg_DelBot,        1,
    NetMsg_InitAI,        0,
    NetMsg_FromAI,        2,
    NetMsg_BotLimit,      2,
    NetMsg_BotBalance,    2,
    NetMsg_MapCRC,        0,
    NetMsg_CheckMaps,     1,
    NetMsg_SwitchName,    0,
    NetMsg_SwitchModel,   2,
    NetMsg_SwitchColor,   2,
    NetMsg_SwitchTeam,    2,
    NetMsg_ServerCommand, 0,
    NetMsg_DemoPacket,    0,

    NetMsg_GetScore,      0,
    NetMsg_GetRoundTimer, 1,
    -1
};

enum
{
    Port_LanInfo = 42067,
    Port_Master  = 42068,
    Port_Server  = 42069,
    ProtocolVersion = 2,              // bump when protocol changes
};

const int maxnamelength = 15;

enum
{
    HudIcon_RedFlag = 0,
    HudIcon_BlueFlag,

    HudIcon_Size    = 120,
};

//you will buffer overflow the rays vector if rays > MAXRAYS
const int MAXRAYS = 16,
          EXP_SELFDAMDIV = 2;
const float EXP_SELFPUSH  = 2.5f,
            EXP_DISTSCALE = 0.5f;
// this defines weapon properties
//                            1    2       3     4         5        6      7         8            9       10      11      12         13          14    15    16       17      18       19   20     21    22       23
const struct attackinfo { int gun, action, anim, vwepanim, hudanim, sound, hudsound, attackdelay, damage, spread, margin, projspeed, kickamount, time, rays, hitpush, exprad, worldfx, use, water, heat, maxheat, gravity;} attacks[Attack_NumAttacks] =
//    1            2          3           4               5             6                7               8    9   10  11   12   13  14    15  16    17 18 19 20  21   22   23
{
    { Gun_Rail,    Act_Shoot, Anim_Shoot, Anim_VWepShoot, Anim_GunShoot, Sound_Rail1,    Sound_Rail2,    300,  5,  20, 0,    0, 10, 1200,  1,  200,  0, 0, 0, 1,  60, 100, 0},
    { Gun_Pulse,   Act_Shoot, Anim_Shoot, Anim_VWepShoot, Anim_GunShoot, Sound_Pulse1,   Sound_Pulse2,   700,  8,  10, 1,    7, 50, 9000,  1, 2500, 50, 1, 0, 0, 200, 300, 4},
    { Gun_Eng,     Act_Shoot, Anim_Shoot, Anim_VWepShoot, Anim_GunShoot, Sound_Melee,    Sound_Melee,    250,  0,   0, 1,    0,  0,   80,  1,   10, 20, 2, 0, 1,  10, 100, 0},
    { Gun_Shotgun, Act_Shoot, Anim_Shoot, Anim_VWepShoot, Anim_GunShoot, Sound_Shotgun1, Sound_Shotgun1, 750,  1, 120, 0,    0, 10,  384, 12,  100,  0, 0, 0, 1,  20, 100, 0},
    { Gun_Carbine, Act_Shoot, Anim_Shoot, Anim_VWepShoot, Anim_GunShoot, Sound_Carbine1, Sound_Carbine1,  90,  2, 120, 0,    0,  2,  512,  1,   50,  0, 0, 0, 1,  25, 125, 0},
};

const struct guninfo { const char *name, *file, *vwep; int attacks[Act_NumActs]; } guns[Gun_NumGuns] =
{
    { "railgun", "railgun", "worldgun/railgun", { -1, Attack_RailShot } },
    { "pulse rifle", "pulserifle", "worldgun/pulserifle", { -1, Attack_PulseShoot } },
    { "engineer rifle", "enggun", "worldgun/pulserifle", { -1, Attack_EngShoot } },
    { "shotgun", "scattergun", "worldgun/carbine", { -1, Attack_ShotgunShoot } },
    { "carbine", "carbine", "worldgun/carbine", { -1, Attack_CarbineShoot } },
};

#include "ai.h"

// inherited by gameent and server clients
struct gamestate
{
    int health, maxhealth;
    int gunselect, gunwait;
    int ammo[Gun_NumGuns], heat[Gun_NumGuns];

    int aitype, skill;
    int combatclass;

    gamestate() : maxhealth(10), aitype(AI_None), skill(0)
    {
        for(int i = 0; i < Gun_NumGuns; i++)
        {
            heat[i] = 0;
        }
    }

    //function neutered because no ents ingame atm
    bool canpickup(int)
    {
        return false;
    }

    //function neutered because no ents ingame atm
    void pickup(int)
    {
    }

    void respawn()
    {
        health = maxhealth;
        gunselect = Gun_Rail;
        gunwait = 0;
        for(int i = 0; i < Gun_NumGuns; ++i)
        {
            ammo[i] = 0;
        }
    }

    void spawnstate()
    {
        if(combatclass == 0)
        {
            gunselect = Gun_Rail;
        }
        else if(combatclass == 1)
        {
            gunselect = Gun_Pulse;
        }
        else if(combatclass == 2)
        {
            gunselect = Gun_Eng;
        }
        else if(combatclass == 3)
        {
            gunselect = Gun_Shotgun;
        }
        else
        {
            gunselect = Gun_Carbine;
        }
    }

    // just subtract damage here, can set death, etc. later in code calling this
    int dodamage(int damage)
    {
        health -= damage;
        return damage;
    }

    int hasammo(int gun, int exclude = -1)
    {
        return validgun(gun) && gun != exclude && ammo[gun] > 0;
    }
};

constexpr int parachutemaxtime = 10000, //time until parachute cancels after spawning
              parachutespeed = 150, //max speed in cubits/s with parachute activated
              defaultspeed = 35;  //default walk speed

constexpr float parachutekickfactor = 0.25; //reduction in weapon knockback with chute dactivated

constexpr int clientlimit = 128;
constexpr int maxtrans = 5000;

const int maxteams = 2;
const char * const teamnames[1+maxteams]     = { "", "azul", "rojo" };
const char * const teamtextcode[1+maxteams]  = { "^f0", "^f1", "^f3" };
const char * const teamblipcolor[1+maxteams] = { "_neutral", "_blue", "_red" };
const int teamtextcolor[1+maxteams] = { 0x1EC850, 0x6496FF, 0xFF4B19 };
const int teamscoreboardcolor[1+maxteams] = { 0, 0x3030C0, 0xC03030 };
inline int teamnumber(const char *name)
{
    for(int i = 0; i < maxteams; ++i)
    {
        if(!strcmp(teamnames[1+i], name))
        {
            return 1+i;
        }
    }
    return 0;
}

inline int validteam(int n)
{
    return (n) >= 1 && (n) <= maxteams;
}

inline const char * teamname(int n)
{
    return teamnames[validteam(n) ? (n) : 0];
}

struct gameent : dynent, gamestate
{
    int weight;                         // affects the effectiveness of hitpush
    int clientnum, privilege, lastupdate, plag, ping;
    int lifesequence;                   // sequence id for each respawn, used in damage test
    int respawned, suicided;
    int lastpain;
    int lastaction, lastattack;
    int attacking;
    int lastpickup, lastpickupmillis, flagpickup;
    int frags, deaths, totaldamage, totalshots, score;
    editinfo *edit;
    float deltayaw, deltapitch, deltaroll, newyaw, newpitch, newroll;
    int smoothmillis;
    int combatclass;
    int parachutetime; //time when parachute spawned
    bool spawnprotect; //if the player has not yet moved
    string name, info;
    int team, playermodel, playercolor;
    ai::aiinfo *ai;
    int ownernum, lastnode;
    int sprinting;

    vec muzzle;

    gameent() : weight(100), clientnum(-1), privilege(Priv_None), lastupdate(0),
                plag(0), ping(0), lifesequence(0), respawned(-1), suicided(-1),
                lastpain(0), frags(0), deaths(0), totaldamage(0),
                totalshots(0), edit(nullptr), smoothmillis(-1), team(0),
                playermodel(-1), playercolor(0), ai(nullptr), ownernum(-1),
                sprinting(1), muzzle(-1, -1, -1)
    {
        name[0] = info[0] = 0;
        respawn();
        //overwrite dynent phyical parameters
        radius = 3.0f;
        eyeheight = 15;
        maxheight = 15;
        aboveeye = 0;
        xradius = 3.0f;
        yradius = 1.0f;
    }
    ~gameent()
    {
        freeeditinfo(edit);
        if(ai) delete ai;
    }

    void hitpush(int damage, const vec &dir, const gameent *actor, int atk)
    {
        vec push(dir);
        push.mul((actor==this && attacks[atk].exprad ? EXP_SELFPUSH : 1.0f)*attacks[atk].hitpush*damage/weight);
        vel.add(push);
    }

    void respawn()
    {
        dynent::reset();
        gamestate::respawn();
        respawned = suicided = -1;
        lastaction = 0;
        lastattack = -1;
        attacking = Act_Idle;
        lastpickup = -1;
        lastpickupmillis = 0;
        parachutetime = lastmillis;
        flagpickup = 0;
        lastnode = -1;
        spawnprotect = true;
    }

    void startgame()
    {
        frags = deaths = 0;
        totaldamage = totalshots = 0;
        maxhealth = 1;
        lifesequence = -1;
        respawned = suicided = -2;
    }
};

struct teamscore
{
    int team, score;
    teamscore() {}
    teamscore(int team, int n) : team(team), score(n) {}

    static bool compare(const teamscore &x, const teamscore &y)
    {
        if(x.score > y.score)
        {
            return true;
        }
        if(x.score < y.score)
        {
            return false;
        }
        return x.team < y.team;
    }
};

struct teaminfo
{
    int frags, score;

    teaminfo()
    {
        reset();
    }

    void reset()
    {
        frags = 0;
        score = 0;
    }
};

namespace entities
{
    extern void editent(int i, bool local);
    extern void resetspawns();
    extern void putitems(packetbuf &p);
    extern void setspawn(int i, bool on);
}

enum
{
    Edit_Face = 0,
    Edit_Tex,
    Edit_Mat,
    Edit_Flip,
    Edit_Copy,
    Edit_Paste,
    Edit_Rotate,
    Edit_Replace,
    Edit_DelCube,
    Edit_AddCube,
    Edit_CalcLight,
    Edit_Remip,
    Edit_VSlot,
    Edit_Undo,
    Edit_Redo
};

extern bool boxoutline;

extern void mpeditent(int i, const vec &o, int type, int attr1, int attr2, int attr3, int attr4, int attr5, bool local);
extern void entdrag(const vec &ray);

extern void modifygravity(gameent *pl, bool water, int curtime);
extern void moveplayer(gameent *pl, int moveres, bool local);
extern bool moveplayer(gameent *pl, int moveres, bool local, int curtime);

extern vec getselpos();
extern void recomputecamera();
extern void findplayerspawn(dynent *d, int forceent, int tag);
extern void crouchplayer(physent *pl, int moveres, bool local);
extern void updatephysstate(physent *d);

namespace game
{
    extern int gamemode;

    struct clientmode
    {
        virtual ~clientmode() {}

        virtual void preload() {}
        virtual void drawhud(gameent *, int, int) {}
        virtual void rendergame() {}
        virtual void respawned(gameent *) {}
        virtual void setup() {}
        virtual void checkitems(gameent *) {}
        virtual int respawnwait(gameent *) { return 0; }
        virtual void pickspawn(gameent *d) { findplayerspawn(d, -1, modecheck(gamemode, Mode_Team) ? d->team : 0); }
        virtual void senditems(packetbuf &) {}
        virtual void removeplayer(gameent *) {}
        virtual void gameover() {}
        virtual bool hidefrags() { return false; }
        virtual int getteamscore(int) { return 0; }
        virtual void getteamscores(std::vector<teamscore> &) {}
        virtual void aifind(gameent *, ai::aistate &, std::vector<ai::interest> &) {}
        virtual bool aicheck(gameent *, ai::aistate &) { return false; }
        virtual bool aidefend(gameent *, ai::aistate &) { return false; }
        virtual bool aipursue(gameent *, ai::aistate &) { return false; }
    };

    extern clientmode *cmode;
    extern void setclientmode();

    // game
    extern bool intermission;
    extern int maptime, maplimit;
    extern gameent *player1;
    extern std::vector<gameent *> players, clients;
    extern int lasthit;
    extern int smoothmove, smoothdist;
    extern int hitsound;

    extern bool allowedittoggle(bool message = true);
    extern void edittrigger(const selinfo &sel, int op, int arg1 = 0, int arg2 = 0, int arg3 = 0, const VSlot *vs = nullptr);
    extern gameent *getclient(int cn);
    extern gameent *newclient(int cn);
    extern const char *colorname(const gameent *d, const char *name = nullptr, const char *alt = nullptr, const char *color = "");
    extern const char *teamcolorname(const gameent *d, const char *alt = "you");
    extern const char *teamcolor(const char *prefix, const char *suffix, int team, const char *alt);
    extern void teamsound(bool sameteam, int n, const vec *loc = nullptr);
    extern void teamsound(gameent *d, int n, const vec *loc = nullptr);
    extern gameent *pointatplayer();
    extern gameent *hudplayer();
    extern gameent *followingplayer();
    extern void stopfollowing();
    extern void checkfollow();
    extern void nextfollow(int dir = 1);
    extern void clientdisconnected(int cn, bool notify = true);
    extern void clearclients(bool notify = true);
    extern void startgame();
    extern void spawnplayer(gameent *);
    extern void deathstate(gameent *d, bool restore = false);
    extern void damaged(int damage, gameent *d, const gameent *actor, bool local = true);
    extern void killed(gameent *d, const gameent *actor);
    extern void timeupdate(int timeremain);
    extern void msgsound(int n, physent *d = nullptr);
    extern void drawicon(int icon, float x, float y, float sz = 120);
    const char *mastermodecolor(int n, const char *unknown);
    const char *mastermodeicon(int n, const char *unknown);
    extern void suicide(physent *d);
    extern void vartrigger(ident *id);

    //minimap
    extern void drawminimap(gameent *d, float x, float y, float s);
    extern void drawteammates(gameent *d, float x, float y, float s);
    extern void drawplayerblip(gameent *d, float x, float y, float s, float blipsize = 1);
    extern void setradartex();
    extern void updateminimap();

    // client
    extern bool connected, demoplayback;
    extern string servdesc;

    extern int parseplayer(const char *arg);
    extern void ignore(int cn);
    extern void unignore(int cn);
    extern bool isignored(int cn);
    extern bool addmsg(int type, const char *fmt = nullptr, ...);
    extern void switchname(const char *name);
    extern void switchteam(const char *name);
    extern void sendmapinfo();
    extern void stopdemo();
    extern void c2sinfo(bool force = false);
    extern void sendposition(gameent *d, bool reliable = false);

    // weapon
    extern int spawncombatclass;
    extern int getweapon(const char *name);
    extern void checkclass();
    extern bool weaponallowed(int weapon, gameent * player = player1); //returns whether weapon is valid for player's class
    extern void shoot(gameent *d, const vec &targ);
    extern void shoteffects(int atk, const vec &from, const vec &to, gameent *d, bool local, int id, int prevaction);
    extern void explode(bool local, gameent *owner, const vec &v, const vec &vel, dynent *safe, int dam, int atk);
    extern void explodeeffects(int atk, const gameent *d, bool local, int id = 0);
    extern void damageeffect(int damage, const gameent *d, bool thirdperson = true);
    extern float intersectdist;
    extern bool intersect(dynent *d, const vec &from, const vec &to, float margin = 0, float &dist = intersectdist);
    extern dynent *intersectclosest(const vec &from, const vec &to, gameent *at, float margin = 0, float &dist = intersectdist);
    extern void clearbouncers();
    extern void updatebouncers(int curtime);
    extern void removebouncers(gameent *owner);
    extern void renderbouncers();
    extern void clearprojectiles();
    extern void updateprojectiles(int curtime);
    extern void removeprojectiles(gameent *owner);
    extern void removeweapons(gameent *owner);
    extern void updateweapons(int curtime);
    extern void gunselect(int gun, gameent *d);
    extern void weaponswitch(gameent *d);
    extern void avoidweapons(ai::avoidset &obstacles, float radius);
    extern void createrays(int atk, const vec &from, const vec &to);

    // scoreboard
    extern int deathscore;
    extern teaminfo teaminfos[maxteams];
    extern void showscores(bool on);
    extern void getbestplayers(std::vector<gameent *> &best);
    extern void getbestteams(std::vector<int> &best);
    extern void clearteaminfo();
    extern void setteaminfo(int team, int frags);
    extern void removegroupedplayer(gameent *d);

    // render
    struct playermodelinfo
    {
        const char *model[1+maxteams], *hudguns[1+maxteams],
                   *icon[1+maxteams];
        bool ragdoll;
    };

    extern void saveragdoll(gameent *d);
    extern void clearragdolls();
    extern void moveragdolls();
    extern const playermodelinfo &getplayermodelinfo(gameent *d);
    extern int getplayercolor(gameent *d, int team);
    extern int chooserandomplayermodel(int seed);
    extern void syncplayer();
    extern void swayhudgun(int curtime);
    extern vec hudgunorigin(int gun, const vec &from, const vec &to, const gameent *d);
    extern void rendergame();
    extern void renderavatar();
    extern void rendereditcursor();
    extern void renderhud();
    extern void renderclient(dynent *d, const char *mdlname, modelattach *attachments, int hold, int attack, int attackdelay, int lastaction, int lastpain, float scale = 1, bool ragdoll = false, float trans = 1);

    // additional fxns needed by server/main code
    extern void gamedisconnect(bool cleanup);
    extern void parsepacketclient(int chan, packetbuf &p);
    extern void connectattempt(const char *name, const char *password, const ENetAddress &address);
    extern void connectfail();
    extern void gameconnect(bool _remote);
    extern void changemap(const char *name);
    extern void preloadworld();
    extern void startmap(const char *name);
    extern bool ispaused();

    extern const char *gameconfig();
    extern const char *savedservers();
    extern void loadconfigs();
    extern int selectcrosshair();

    extern void updateworld();
    extern void initclient();
    extern int scaletime(int t);
    extern const char *getmapinfo();
    extern const char *gameident();
    extern const char *defaultconfig();
}

//main
extern void updateenginevalues();

// game
extern int thirdperson;
extern bool isthirdperson();
extern void modifyvelocity(physent *pl, bool local, bool water, bool floating, int curtime);

// server
extern ENetAddress masteraddress;

extern void updatetime();

extern ENetSocket connectmaster(bool wait);

extern ENetPacket *sendfile(int chan, stream *file, const char *format = "", ...);
extern const char *disconnectreason(int reason);
extern void closelogfile();
extern void setlogfile(const char *fname);

// client
extern ENetPeer *curpeer;
extern void localservertoclient(int chan, ENetPacket *packet);
extern void connectserv(const char *servername, int port, const char *serverpassword);
extern void abortconnect();

extern void sendclientpacket(ENetPacket *packet, int chan);
extern void flushclient();
extern void disconnect(bool async = false, bool cleanup = true);
extern const ENetAddress *connectedpeer();
extern void neterr(const char *s, bool disc = true);
extern void gets2c();
extern void notifywelcome();

// edit

extern void mpeditface(int dir, int mode, selinfo &sel, bool local);
extern bool mpedittex(int tex, int allfaces, selinfo &sel, ucharbuf &buf);
extern void mpeditmat(int matid, int filter, selinfo &sel, bool local);
extern void mpflip(selinfo &sel, bool local);
extern void mpcopy(editinfo *&e, selinfo &sel, bool local);
extern void mppaste(editinfo *&e, selinfo &sel, bool local);
extern void mprotate(int cw, selinfo &sel, bool local);
extern bool mpreplacetex(int oldtex, int newtex, bool insel, selinfo &sel, ucharbuf &buf);
extern void mpdelcube(selinfo &sel, bool local);
extern void mpplacecube(selinfo &sel, int tex, bool local);
extern void mpremip(bool local);
extern bool mpeditvslot(int delta, int allfaces, selinfo &sel, ucharbuf &buf);
extern void mpcalclight(bool local);

extern uint getfacecorner(uint face, int num);
extern int shouldpacktex(int index);
extern bool unpackundo(const uchar *inbuf, int inlen, int outlen);

extern editinfo *localedit;

struct facearray
{
    int array[8];
};

extern facearray facestoarray(cube c, int num);
extern bool checkcubefill(cube c);

// ents

extern undoblock *copyundoents(undoblock *u);
extern void pasteundoents(undoblock *u);
extern void pasteundoent(int idx, const entity &ue);
extern bool hoveringonent(int ent, int orient);
extern void renderentselection(const vec &o, const vec &ray, bool entmoving);

// serverbrowser

extern servinfo *getservinfo(int i);

#define GETSERVINFO(idx, si, body) do { \
    servinfo *si = getservinfo(idx); \
    if(si) \
    { \
        body; \
    } \
} while(0)

#define GETSERVINFOATTR(idx, aidx, aval, body) \
    GETSERVINFO(idx, si, \
    { \
        if(si->attr.size() > aidx) \
        { \
            int aval = si->attr[aidx]; \
            body; \
        } \
    })

extern bool resolverwait(const char *name, ENetAddress *address);
extern int connectwithtimeout(ENetSocket sock, const char *hostname, const ENetAddress &address);
extern void addserver(const char *name, int port = 0, const char *password = nullptr, bool keep = false);
extern void writeservercfg();

namespace server
{
    extern const char *modename(int n, const char *unknown = "unknown");
    extern const char *modeprettyname(int n, const char *unknown = "unknown");
    extern const char *mastermodename(int n, const char *unknown = "unknown");
    extern void hashpassword(int cn, int sessionid, const char *pwd, char *result, int maxlen = maxstrlen);
    extern int msgsizelookup(int msg);
    extern bool serveroption(const char *arg);

    extern int numchannels();
    extern void recordpacket(int chan, void *data, int len);
    extern const char *defaultmaster();
}

#endif

