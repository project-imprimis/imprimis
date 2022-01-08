// main.cpp: initialisation & main loop

#include <fstream>
#include "game.h"

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
        extern int fullscreen,
            sound,
            soundchans,
            soundfreq,
            soundbufferlen;

        cfgfile << "// This file is written automatically on exit.\n"
            << "// Any changes to this file WILL be overwritten.\n\n"
            << "fullscreen " << fullscreen << "\n"
            << "screenw " << scr_w << "\n"
            << "screenh " << scr_h << "\n"
            << "sound " << sound << "\n"
            << "soundchans " << soundchans << "\n"
            << "soundfreq " << soundfreq << "\n"
            << "soundbufferlen " << soundbufferlen << "\n";
        if(audiodriver[0]) // Omit line if no audio driver is present
        {
            cfgfile << "audiodriver " << escapestring(audiodriver) << "\n"; // Replace call to ``escapestring`` with C++ standard method?
        }
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
    writecfg(game::defaultconfig());
    SDL_ShowCursor(SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
    if(screen)
    {
        SDL_SetWindowGrab(screen, SDL_FALSE);
    }
    cleargamma();
    freeocta(worldroot);
    UI::cleanup();
    clear_command();
    clear_console();
    clear_models();
    clear_sound();
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
    numdynents = game::players.length();
    allowediting = game::allowedittoggle(false);
    multiplayer = curpeer;
    //pass players list
    vector<dynent *> dyns;
    for(int i = 0; i < game::players.length(); ++i)
    {
        dyns.add(game::players[i]);
    }
    dynents = dyns;
}

constexpr const char * versionstring = "Alpha 13 \"Malachite\"";

//sets engine constants that need information from the game
//as a result, all values set here are global variables defined elsewhere in
//global scope
void startupconstants()
{
    numanims = Anim_NumAnims;
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

/**
 * @brief Runs the game. Also, sets the home directory and log file.
 * @param argc The number of arguments. This is equal or greater than 1, because
 *             the executable counts as one argument.
 * @param argv The list or arguments. The 0th argument is the executable name,
 *             and the 1st, 2nd, 3rd, etc are arguments for the executable.
 * @return int Returns 0 if the program exited normally.
 */
int main(int argc, char **argv)
{
    initidents();
    setlogfile(nullptr);
    startupconstants();
    initing = Init_Reset;

    // set home dir first
    for(int i = 1; i<argc; i++)
    {
        // If the argument starts with -u
        if(argv[i][0]=='-' && argv[i][1] == 'u')
        {
            // Set the home directory.
            sethomedir(&argv[i][2]);
            break;
        }
    }
    // set log after home dir, but before anything else
    for(int i = 1; i < argc; i++)
    {
        // If the argument starts with -g
        if(argv[i][0]=='-' && argv[i][1] == 'g')
        {
            // Skip the -g flag and use the rest of the argument as the file
            // name if possible. Examples:
            // -g         -> log.txt
            // -ggame.txt -> game.txt
            // -glog.txt  -> log.txt
            const char *file = argv[i][2] ? &argv[i][2] : "log.txt";
            setlogfile(file);
            logoutf("Setting log file: %s", file);
            break;
        }
    }
    execfile("config/init.cfg", false);
    //init SDL display/input library
    logoutf("init: sdl");
    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|SDL_INIT_AUDIO)<0)
    {
        fatal("Unable to initialize SDL: %s", SDL_GetError());
    }
    //init enet networking library
    logoutf("init: net");
    if(enet_initialize()<0)
    {
        fatal("Unable to initialise network module");
    }
    atexit(enet_deinitialize);
    enet_time_set(0);
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
    emptymap(0, true, nullptr, false);
    game::startmap(nullptr);
    logoutf("init: sound");
    initsound();

    logoutf("init: cfg");
    initing = Init_Load;
    //run startup scripts
    execfile("config/keymap.cfg");
    execfile("config/stdedit.cfg");
    execfile(game::gameconfig());
    execfile("config/sound.cfg");
    execfile("config/ui.cfg");
    execfile("config/heightmap.cfg");
    //server list
    if(game::savedservers())
    {
        execfile(game::savedservers(), false);
    }
    identflags |= Idf_Persist;
    //personal configs
    execfile(game::defaultconfig());
    writecfg(game::defaultconfig());
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

        checkinput(); //go and see if SDL has any new input: mouse, keyboard, screen dimensions
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
        updateparticles();
        updatesounds();

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
