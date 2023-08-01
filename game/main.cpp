// main.cpp: initialisation & main loop

#include <fstream>
#include "game.h"
#include "sound.h"

void writeinitcfg()
{
    std::ofstream cfgfile;
    if(homedir[0]) // Verify that the home directory is set
    {
        cfgfile.open(static_cast<std::string>(homedir) + std::string("config/init.cfg"), std::ios::trunc);
    }
    if(cfgfile.is_open())
    {
        // Import all variables to write out to the config file
        extern char *audiodriver;
        extern int fullscreen;

        cfgfile << "// This file is written automatically on exit.\n"
            << "// Any changes to this file WILL be overwritten.\n\n"
            << "name " << game::player1->name << "\n"
            << "fullscreen " << fullscreen << "\n"
            << "screenw " << scr_w << "\n"
            << "screenh " << scr_h << "\n"
            << "sound " << soundmain.getsound() << "\n"
            << "soundchans " << soundmain.getsoundchans() << "\n";
        cfgfile.close();
    }
}

// normal exit, saves config
void quit()
{
    writeinitcfg();
    writeservercfg();
    abortconnect();
    disconnect();
    writecfg((std::string(homedir) + std::string(game::defaultconfig())).c_str());
    SDL_ShowCursor(SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    if(screen)
    {
        SDL_SetWindowGrab(screen, SDL_FALSE);
    }
    cleargamma();
    freeocta(rootworld.worldroot);
    UI::cleanup();
    clear_command();
    clear_console();
    clear_models();
    soundmain.clear_sound();
    closelogfile();
    SDL_Quit();
    exit(EXIT_SUCCESS);
}
COMMAND(quit, "");

//updates per-frame global variable constants
//these are set by the game but may change (e.g. the game requests adding or removing a player)
//but are engine values defined in the global scope
void updateenginevalues()
{
    numdynents = game::players.size();
    allowediting = game::allowedittoggle(false);
    multiplayer = curpeer;
    //pass players list
    std::vector<dynent *> dyns;
    for(uint i = 0; i < game::players.size(); ++i)
    {
        dyns.push_back(game::players[i]);
    }
    dynents = dyns;
}

constexpr const char * versionstring = "Alpha 32 \"Foss\"";

//sets engine constants that need information from the game
//as a result, all values set here are global variables defined elsewhere in
//global scope
void startupconstants()
{
    std::string animlist[] =
    {
        "mapmodel",
        "dead", "dying",
        "idle", "run N", "run NE", "run E", "run SE", "run S", "run SW", "run W", "run NW",
        "jump", "jump N", "jump NE", "jump E", "jump SE", "jump S", "jump SW", "jump W", "jump NW",
        "sink", "swim",
        "crouch", "crouch N", "crouch NE", "crouch E", "crouch SE", "crouch S", "crouch SW", "crouch W", "crouch NW",
        "crouch jump", "crouch jump N", "crouch jump NE", "crouch jump E", "crouch jump SE", "crouch jump S", "crouch jump SW", "crouch jump W", "crouch jump NW",
        "crouch sink", "crouch swim",
        "shoot",
        "pain",
        "edit", "lag", "win", "lose",
        "gun idle", "gun shoot",
        "vwep idle", "vwep shoot",
    };
    animnames = std::vector<std::string>(std::begin(animlist), std::end(animlist));

    std::string entnamelist[GamecodeEnt_MaxEntTypes] =
    {
        "none?", "light", "mapmodel", "playerstart", "particles", "sound", "spotlight", "decal"
    };
    entnames = std::vector<std::string>(std::begin(entnamelist), std::end(entnamelist));
}

void menuprocess()
{
    static int lastmainmenu = -1;
    if(lastmainmenu != mainmenu)
    {
        lastmainmenu = mainmenu;
        execident("mainmenutoggled");
    }
    if(mainmenu && !multiplayer && !UI::hascursor())
    {
        UI::showui("main");
    }
}

int main(int argc, char **argv)
{
    initidents();
    setlogfile(nullptr);
    startupconstants();
    initing = Init_Reset;
    // set home dir first
    for(int i = 1; i<argc; i++)
    {
        if(argv[i][0]=='-' && argv[i][1] == 'u')
        {
            sethomedir(&argv[i][2]);
            logoutf("Setting home folder: %s", &argv[i][2]);
            break;
        }
    }
    // set log after home dir, but before anything else
    for(int i = 1; i < argc; i++)
    {
        if(argv[i][0]=='-' && argv[i][1] == 'g')
        {
            const char *file = argv[i][2] ? &argv[i][2] : "log.txt";
            setlogfile(file);
            logoutf("Setting log file: %s", file);
            break;
        }
    }
    //init SDL display/input library
    logoutf("init: sdl");
    if(!initsdl()) //initalize sdl from engine library
    {
        fatal("Unable to initialize SDL: %s", SDL_GetError());
    }
    SDL_GameController* stick = SDL_GameControllerOpen(0);
    printf("%s found\n", SDL_GameControllerName(stick));
    //init enet networking library
    logoutf("init: net");
    if(enet_initialize()<0)
    {
        fatal("Unable to initialise network module");
    }
    atexit(enet_deinitialize);
    enet_time_set(0);
    logoutf("init: cubescript");
    initcscmds();
    initmathcmds();
    initcontrolcmds();
    initstrcmds();
    inittextcmds();
    initconsolecmds();
    initoctaeditcmds();
    initoctaworldcmds();
    initoctaworldcmds();
    initrendermodelcmds();
    initrenderglcmds();
    initrenderlightscmds();
    initrendertextcmds();
    initrenderwindowcmds();
    initshadercmds();
    inittexturecmds();
    inithudcmds();
    initsoundcmds();
    initheightmapcmds();
    initmenuscmds();
    initworldiocmds();
    initzipcmds();

    logoutf("init: game");
    game::initclient();
    logoutf("init: video");
    SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "0");
    #if !defined(WIN32) //*nix
        SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    #endif
    setupscreen();
    SDL_ShowCursor(SDL_FALSE);
    SDL_StopTextInput(); // workaround for spurious text-input events getting sent on first text input toggle?
    logoutf("init: gl");
    conoutf(Console_Init, "Version: %s built %s", versionstring, __DATE__);
    conoutf(Console_Init, "Engine: %s built %s", enginestr().c_str(), enginebuilddate().c_str());
    gl_checkextensions();
    gl_init();
    notexture = textureload("media/texture/game/notexture.png");
    if(!notexture)
    {
        fatal("could not find core textures");
    }
    logoutf("init: console");
    if(!execfile("config/stdlib.cfg", false))
    {
        fatal("cannot find data files (you are running from the wrong folder, try .bat file in the main folder)");   // this is the first file we load.
    }
    if(!execfile("config/font.cfg", false))
    {
        fatal("cannot find font definitions");
    }
    if(!setfont("default"))
    {
        fatal("no default font specified");
    }
    UI::setup();

    inbetweenframes = true;
    renderbackground("initializing...");

    logoutf("init: world");
    updateenginevalues();
    camera1 = player = iterdynents(0);
    rootworld.emptymap(0, true, false);
    game::startmap(nullptr);
    logoutf("init: sound");
    soundmain.initsound();
    logoutf("init: ui");
    UI::inituicmds();
    logoutf("init: cfg");
    initing = Init_Load;
    //run startup scripts
    execfile("config/init.cfg", false);

    execfile("config/keymap.cfg");
    execfile("config/stdedit.cfg");
    execfile(game::gameconfig());
    execfile("config/sound.cfg");
    execfile("config/ui.cfg");
    execfile("config/heightmap.cfg");
    execident("applychanges");
    //server list
    if(game::savedservers())
    {
        execfile(game::savedservers(), false);
    }
    identflags |= Idf_Persist;
    //personal configs
    execfile(game::defaultconfig());
    writecfg((std::string(homedir) + std::string(game::defaultconfig())).c_str());
    identflags &= ~Idf_Persist;
    initing = Init_Game;
    game::loadconfigs();
    initing = Init_Not;
    logoutf("init: render");
    //setup renderer
    restoregamma();
    restorevsync();
    initgbuffer();
    loadshaders();
    initparticles();
    initstains();
    initoctaeditcmds();
    identflags |= Idf_Persist;

    logoutf("init: mainloop");
    resetfpshistory();
    inputgrab(grabinput = true);
    ignoremousemotion();
    //actual loop after main inits itself
    for(;;)
    {
        updateenginevalues();
        if(!triggerqueue.empty())
        {
            game::vartrigger(triggerqueue.front()); //skim top of pending var changes to apply gameside
            triggerqueue.pop();
        }
        static int frames = 0;
        int millis = getclockmillis(); //gets time at loop
        limitfps(millis, totalmillis); //caps framerate if necessary
        elapsedtime = millis - totalmillis;
        static int timeerr = 0;
        int scaledtime = game::scaletime(elapsedtime) + timeerr;
        curtime = scaledtime/100;
        timeerr = scaledtime%100;
        if(!multiplayer && curtime>200)
        {
            curtime = 200;
        }
        if(game::ispaused())
        {
            curtime = 0;
        }
        lastmillis += curtime;
        totalmillis = millis;
        updatetime();

        //go and see if SDL has any new input: mouse, keyboard, screen dimensions
        if(!mainmenu)
        {
            if(editmode)
            {
                checkinput(1);
            }
            else if(player->state==ClientState_Spectator)
            {
                checkinput(2);
            }
            else
            {
                checkinput(0);
            }
        }
        else
        {
            checkinput(0);
        }

        UI::update(); //checks cursor and updates uis
        menuprocess(); //shows main menu if not ingame and not online

        if(lastmillis)
        {
            game::updateworld(); //main ingame update routine: calculates projectile positions, physics, etc.
        }
        checksleep(lastmillis); //checks cubescript for any pending sleep commands
        if(frames)
        {
            updatefpshistory(elapsedtime); //if collecting framerate history, update with new frame
        }
        frames++;
        // miscellaneous general game effects
        recomputecamera();
        rootworld.updateparticles();
        soundmain.updatesounds();

        if(minimized)
        {
            continue; //let's not render a frame unless there's a screen to be seen
        }
        gl_setupframe(!mainmenu); //also, don't need to set up a frame if on the static main menu

        inbetweenframes = false; //tell other stuff that the frame is starting
        int crosshairindex = game::selectcrosshair();
        //create pointers to the game & hud rendering we want the renderer to slip in
        void (*gamefxn)() = &game::rendergame;
        void (*hudfxn)() = &game::renderavatar;
        void (*editfxn)() = &game::rendereditcursor;
        void (*hud2dfxn)() = &game::renderhud;
        gl_drawframe(crosshairindex, gamefxn, hudfxn, editfxn, hud2dfxn); //rendering magic
        swapbuffers();
        game::updateminimap();
        renderedframe = inbetweenframes = true; //done!
    }
    return EXIT_FAILURE;
}
