
#include "game.h"
#include "sound.h"

SoundEngine soundmain;

//static bindings for methods
//cubescript bindings can't bind to method pointers, so here they are bound to free functions

void startmusic(char* name, char* cmd)
{
    soundmain.startmusic(name, cmd);
}

void playsound(const int* const i)
{
    soundmain.playsound(*i);
} 

void resetsound()
{
    soundmain.resetsound();
}

void registersound(char *name, int *vol)
{
    soundmain.registersound(name, vol);
}

void mapsound(char *name, int *vol, int *maxuses)
{
    soundmain.mapsound(name, vol, maxuses);
}

void altsound(char *name, int *vol)
{
    soundmain.altsound(name, vol);
}

void altmapsound(char *name, int *vol)
{
    soundmain.altmapsound(name, vol);
}

void numsounds()
{
    soundmain.numsounds();
}

void nummapsounds()
{
    soundmain.nummapsounds();
}

void soundreset()
{
    soundmain.soundreset();
}

void mapsoundreset()
{
    soundmain.mapsoundreset();
}

void initsoundcmds()
{
    addcommand("music", reinterpret_cast<identfun>(startmusic), "ss", Id_Command);
    addcommand("playsound", reinterpret_cast<identfun>(playsound), "i", Id_Command); //i: the index of the sound to be played
    addcommand("resetsound", reinterpret_cast<identfun>(resetsound), "", Id_Command); //stop all sounds and re-play background music
    addcommand("registersound", reinterpret_cast<identfun>(registersound), "si", Id_Command);
    addcommand("mapsound", reinterpret_cast<identfun>(mapsound), "sii", Id_Command);
    addcommand("altsound", reinterpret_cast<identfun>(altsound), "si", Id_Command);
    addcommand("altmapsound", reinterpret_cast<identfun>(altmapsound), "si", Id_Command);
    addcommand("numsounds", reinterpret_cast<identfun>(numsounds), "", Id_Command);
    addcommand("nummapsounds", reinterpret_cast<identfun>(nummapsounds), "", Id_Command);
    addcommand("soundreset", reinterpret_cast<identfun>(soundreset), "", Id_Command);
    addcommand("mapsoundreset", reinterpret_cast<identfun>(mapsoundreset), "", Id_Command);
}

VARFP(soundvol, 0, 255, 255,
    soundmain.setsoundvol(&soundvol);
);

VARFP(musicvol, 0, 255, 255,
    soundmain.setmusicvol(&soundvol);
);

VARF(sound, 0, 1, 1,
    soundmain.setsound(&sound);
);

VARF(soundchans, 1, 32, 128,
    soundmain.setsoundchans(&soundchans);
);

VARF(stereo, 0, 1, 1,
    soundmain.setstereo(&stereo);
);
