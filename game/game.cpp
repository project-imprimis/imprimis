#include "game.h"
#include "sound.h"

//for activites that can take place ingame, such as shooting, spectating, etc;
//also for:
//player position extrapolation
//updating the whole world
//player statistics (kills, deaths, accuracy)

void physicsframe();

const float jumpvel = 56.0f; //impulse scale for player jump

static VAR(floatspeed, 1, 100, 10000);

namespace game
{
    bool intermission = false;
    int maptime = 0,
        maprealtime = 0,
        maplimit = -1;
    int lasthit = 0;

    gameent *player1 = nullptr;         // our client
    std::vector<gameent *> players;       // other clients

    int following = -1;

    VARFP(specmode, 0, 0, 2,
    {
        if(!specmode)
        {
            stopfollowing();
        }
        else if(following < 0)
        {
            nextfollow();
        }
    });

    gameent *followingplayer()
    {
        if(player1->state!=ClientState_Spectator || following<0)
        {
            return nullptr;
        }
        gameent *target = getclient(following);
        if(target && target->state!=ClientState_Spectator)
        {
            return target;
        }
        return nullptr;
    }

    ICOMMAND(getfollow, "", (),
    {
        gameent *f = followingplayer();
        intret(f ? f->clientnum : -1);
    });

    void stopfollowing()
    {
        if(following<0)
        {
            return;
        }
        following = -1;
    }

    void follow(char *arg)
    {
        int cn = -1;
        if(arg[0])
        {
            if(player1->state != ClientState_Spectator)
            {
                return;
            }
            cn = parseplayer(arg);
            if(cn == player1->clientnum)
            {
                cn = -1;
            }
        }
        if(cn < 0 && (following < 0 || specmode))
        {
            return;
        }
        following = cn;
    }
    COMMAND(follow, "s");

    void nextfollow(int dir)
    {
        if(player1->state!=ClientState_Spectator)
        {
            return;
        }
        int cur = following >= 0 ? following : (dir < 0 ? clients.size() - 1 : 0);
        for(uint i = 0; i < clients.size(); i++)
        {
            cur = (cur + dir + clients.size()) % clients.size();
            if(clients[cur] && clients[cur]->state!=ClientState_Spectator)
            {
                following = cur;
                return;
            }
        }
        stopfollowing();
    }
    ICOMMAND(nextfollow, "i", (int *dir), nextfollow(*dir < 0 ? -1 : 1));

    void checkfollow()
    {
        if(player1->state != ClientState_Spectator)
        {
            if(following >= 0)
            {
                stopfollowing();
            }
        }
        else
        {
            if(following >= 0)
            {
                gameent *d = (static_cast<int>(clients.size()) > following) ? clients[following] : nullptr;
                if(!d || d->state == ClientState_Spectator)
                {
                    stopfollowing();
                }
            }
            if(following < 0 && specmode)
            {
                nextfollow();
            }
        }
    }

    void mapname()
    {
        result(getclientmap());
    }

    COMMAND(mapname, "");

    void savecurrentmap()
    {
        rootworld.save_world(getclientmap(), game::gameident());
    }

    void savemap(char *mname)
    {
        rootworld.save_world(mname, game::gameident());
    }

    COMMAND(savemap, "s");
    COMMAND(savecurrentmap, "");

    gameent *spawnstate(gameent *d)              // reset player state not persistent accross spawns
    {
        d->respawn();
        d->spawnstate();
        return d;
    }

    void respawnself()
    {
        if(ispaused())
        {
            return;
        }
        int seq = (player1->lifesequence<<16)|((lastmillis/1000)&0xFFFF);
        if(player1->respawned!=seq)
        {
            addmsg(NetMsg_TrySpawn, "rc", player1);
            player1->respawned = seq;
        }
    }

    gameent *pointatplayer()
    {
        for(uint i = 0; i < players.size(); i++)
        {
            if(players[i] != player1 && intersect(players[i], player1->o, worldpos))
            {
                return players[i];
            }
        }
        return nullptr;
    }


    bool allowthirdperson()
    {
        return !multiplayer || player1->state==ClientState_Spectator || player1->state==ClientState_Editing || modecheck(gamemode, Mode_Edit);
    }

    gameent *hudplayer()
    {
        if((thirdperson && allowthirdperson()) || specmode > 1)
        {
            return player1;
        }
        gameent *target = followingplayer();
        return target ? target : player1;
    }

    void setupcamera()
    {
        gameent *target = followingplayer();
        if(target)
        {
            player1->yaw = target->yaw;
            player1->pitch = target->state==ClientState_Dead ? 0 : target->pitch;
            player1->o = target->o;
            player1->resetinterp();
        }
    }

    bool detachcamera()
    {
        gameent *d = followingplayer();
        if(d)
        {
            return specmode > 1 || d->state == ClientState_Dead;
        }
        return player1->state == ClientState_Dead;
    }

    bool collidecamera()
    {
        switch(player1->state)
        {
            case ClientState_Editing:
            {
                return false;
            }
            case ClientState_Spectator:
            {
                return followingplayer()!=nullptr;
            }
        }
        return true;
    }

// player prediction variables

    VARP(smoothmove, 0, 75, 100); //divisor for smoothing scale
    VARP(smoothdist, 0, 32, 64); //used in game/client.cpp; distance threshold for player position extrapolation

    void predictplayer(gameent *d, bool move)
    {
        d->o = d->newpos;
        d->yaw = d->newyaw;
        d->pitch = d->newpitch;
        d->roll = d->newroll;
        if(move)
        {
            moveplayer(d, 1, false);
            d->newpos = d->o;
        }
        float k = 1.0f - static_cast<float>(lastmillis - d->smoothmillis)/smoothmove;
        if(k>0)
        {
            d->o.add(vec(d->deltapos).mul(k));
            d->yaw += d->deltayaw*k;
            if(d->yaw<0)
            {
                d->yaw += 360;
            }
            else if(d->yaw>=360)
            {
                d->yaw -= 360;
            }
            d->pitch += d->deltapitch*k;
            d->roll += d->deltaroll*k;
        }
    }

    static void otherplayers()
    {
        for(uint i = 0; i < players.size(); i++)
        {
            gameent *d = players[i];
            if(d == player1 || d->ai)
            {
                continue;
            }
            if(d->state==ClientState_Dead && d->ragdoll)
            {
                moveragdoll(d);
            }
            else if(!intermission)
            {
                if(lastmillis - d->lastaction >= d->gunwait)
                {
                    d->gunwait = 0;
                }
            }
            const int lagtime = totalmillis-d->lastupdate;
            if(!lagtime || intermission)
            {
                continue;
            }
            else if(lagtime>1000 && d->state==ClientState_Alive)
            {
                d->state = ClientState_Lagged;
                continue;
            }
            if(d->state==ClientState_Alive || d->state==ClientState_Editing)
            {
                crouchplayer(d, 10);
                if(smoothmove && d->smoothmillis>0)
                {
                    predictplayer(d, true);
                }
                else
                {
                    moveplayer(d, 1, false);
                }
            }
            else if(d->state==ClientState_Dead && !d->ragdoll && lastmillis-d->lastpain<2000)
            {
                moveplayer(d, 1, true);
            }
        }
    }

    void updateworld()        // main game update loop
    {
        if(!maptime)
        {
            maptime = lastmillis;
            maprealtime = totalmillis;
            return;
        }
        if(!curtime)
        {
            gets2c();
            if(player1->clientnum>=0)
            {
                c2sinfo();
            }
            return;
        }
        physicsframe();
        ai::navigate();
        updateweapons(curtime); //updates projectiles & bouncers
        otherplayers();
        ai::update();
        moveragdolls();
        gets2c(); //get server to client info
        if(connected)
        {
            if(player1->state == ClientState_Dead) //ragdoll check
            {
                if(player1->ragdoll)
                {
                    moveragdoll(player1);
                }
                else if(lastmillis-player1->lastpain<2000)
                {
                    player1->move = player1->strafe = 0;
                    moveplayer(player1, 10, true);
                }
            }
            else if(!intermission) //nobody moves when it's intermission (between games)
            {
                if(player1->ragdoll)
                {
                    cleanragdoll(player1);
                }
                crouchplayer(player1, 10);
                moveplayer(player1, 10, true);
                swayhudgun(curtime);
            }
        }
        if(player1->clientnum>=0)
        {
            c2sinfo();   // do this last, to reduce the effective frame lag
        }
    }

    void spawnplayer(gameent *d)   // place at random spawn
    {
        findplayerspawn(d, -1, modecheck(gamemode, Mode_Team) ? d->team : 0);
        spawnstate(d);
        if(d==player1)
        {
            if(editmode)
            {
                d->state = ClientState_Editing;
            }
            else if(d->state != ClientState_Spectator)
            {
                d->state = ClientState_Alive;
            }
        }
        else
        {
            d->state = ClientState_Alive;
        }
        checkfollow();
    }

    VARP(spawnwait, 0, 0, 1000);

    void respawn()
    {
        if(player1->state==ClientState_Dead)
        {
            checkclass();
            player1->attacking = Act_Idle;
            if(lastmillis < player1->lastpain + spawnwait)
            {
                return;
            }
            respawnself();
        }
    }
    COMMAND(respawn, "");

    // inputs
    VARP(attackspawn, 0, 1, 1);

    void doaction(int act)
    {
        if(!connected || intermission)
        {
            return;
        }
        if((player1->attacking = act) && attackspawn)
        {
            respawn();
            return;
        }
        game::player1->spawnprotect = false;
    }

    ICOMMAND(shoot, "D", (int *down), doaction(*down ? Act_Shoot : Act_Idle));

    VARP(jumpspawn, 0, 1, 1);

    bool canjump()
    {
        if(!connected || intermission)
        {
            return false;
        }
        if(jumpspawn)
        {
            respawn();
        }
        return player1->state!=ClientState_Dead;
    }

    bool cancrouch()
    {
        if(!connected || intermission)
        {
            return false;
        }
        return player1->state!=ClientState_Dead;
    }

    bool cansprint()
    {
        if(!connected || intermission)
        {
            return false;
        }
        return player1->state!=ClientState_Dead;
    }

    VARP(hitsound, 0, 0, 1);

    void damaged(int damage, gameent *d, const gameent *actor, bool local)
    {
        if((d->state!=ClientState_Alive && d->state != ClientState_Lagged && d->state != ClientState_Spawning) || intermission)
        {
            return;
        }
        if(local)
        {
            damage = d->dodamage(damage);
        }
        else if(actor==player1)
        {
            return;
        }
        gameent *h = hudplayer();
        if(h!=player1 && actor==h && d!=actor)
        {
            if(hitsound && lasthit != lastmillis)
            {
                soundmain.playsound(Sound_Hit);
            }
            lasthit = lastmillis;
        }
        if(d==h)
        {
            damageblend(damage);
            damagecompass(damage, actor->o);
        }
        damageeffect(damage, d, d!=h);

        if(d->ai)
        {
            d->ai->damaged(actor);
        }

        if(d->health<=0)
        {
            if(local)
            {
                killed(d, actor);
            }
        }
    }

    VARP(deathscore, 0, 1, 1);

    void deathstate(gameent *d, bool restore)
    {
        d->state = ClientState_Dead;
        d->lastpain = lastmillis;
        if(!restore)
        {
            d->deaths++;
        }
        if(d==player1)
        {
            if(deathscore)
            {
                showscores(true);
            }
            disablezoom();
            d->attacking = Act_Idle;
            //d->pitch = 0;
            d->roll = 0;
            soundmain.playsound(Sound_Die2);
        }
        else
        {
            d->move = d->strafe = 0;
            d->resetinterp();
            d->smoothmillis = 0;
            soundmain.playsound(Sound_Die1, &d->o);
        }
    }

    VARP(teamcolorfrags, 0, 1, 1);

    void killed(gameent *d, const gameent *actor)
    {
        vec dloc = d->o; //need to make a local copy of d->o because sub() is a destructive operation
        int fragdist = static_cast<int>(dloc.sub(actor->o).magnitude()/8);
        if(d->state==ClientState_Editing)
        {
            d->editstate = ClientState_Dead;
            d->deaths++;
            if(d!=player1)
            {
                d->resetinterp();
            }
            return;
        }
        else if((d->state!=ClientState_Alive && d->state != ClientState_Lagged && d->state != ClientState_Spawning) || intermission)
        {
            return;
        }
        gameent *h = followingplayer();
        if(!h)
        {
            h = player1;
        }
        int contype = d==h || actor==h ? ConsoleMsg_FragSelf : ConsoleMsg_FragOther;
        const char *dname = "", *aname = "";
        if(modecheck(gamemode, Mode_Team) && teamcolorfrags)
        {
            dname = teamcolorname(d, "you");
            aname = teamcolorname(actor, "you");
        }
        else
        {
            dname = colorname(d, nullptr, "you");
            aname = colorname(actor, nullptr, "you");
        }
        if(d==actor)
        {
            conoutf(contype, "^f2%s suicided%s", dname, d==player1 ? "!" : "");
        }
        else if(modecheck(gamemode, Mode_Team) && (d->team == actor->team)) //if player is on the same team in a team mode
        {
            contype |= ConsoleMsg_TeamKill;
            if(actor==player1)
            {
                conoutf(contype, "^f6%s fragged a teammate (%s)", aname, dname);
            }
            else if(d==player1)
            {
                conoutf(contype, "^f6%s got fragged by a teammate (%s)", dname, aname);
            }
            else
            {
                conoutf(contype, "^f2%s fragged a teammate (%s)", aname, dname);
            }
        }
        else
        {
            if(d==player1)
            {
                conoutf(contype, "^f2%s got fragged by %s (%dm)", dname, aname, fragdist);
            }
            else
            {
                conoutf(contype, "^f2%s fragged %s (%dm)", aname, dname, fragdist);
            }
        }
        deathstate(d);
        if(d->ai)
        {
            d->ai->killed();
        }
    }

    void timeupdate(int secs)
    {
        if(secs > 0)
        {
            maplimit = lastmillis + secs*1000;
        }
        else
        {
            intermission = true;
            player1->attacking = Act_Idle;
            conoutf(ConsoleMsg_GameInfo, "^f2intermission:");
            conoutf(ConsoleMsg_GameInfo, "^f2game has ended!");
            conoutf(ConsoleMsg_GameInfo, "^f2player frags: %d, deaths: %d, score: %d", player1->frags, player1->deaths, player1->score);
            int accuracy = (player1->totaldamage*100)/max(player1->totalshots, 1);
            conoutf(ConsoleMsg_GameInfo, "^f2player total damage dealt: %d, damage wasted: %d, efficiency(%%): %d", player1->totaldamage, player1->totalshots-player1->totaldamage, accuracy);
            showscores(true);
            disablezoom();
            execident("intermission");
        }
    }

    ICOMMAND(getfrags, "", (), intret(player1->frags));
    ICOMMAND(getscore, "", (), intret(player1->score));
    ICOMMAND(getdeaths, "", (), intret(player1->deaths));
    ICOMMAND(getaccuracy, "", (), intret((player1->totaldamage*100)/max(player1->totalshots, 1)));
    ICOMMAND(gettotaldamage, "", (), intret(player1->totaldamage));
    ICOMMAND(gettotalshots, "", (), intret(player1->totalshots));

    std::vector<gameent *> clients;

    gameent *newclient(int cn)   // ensure valid entity
    {
        if(cn < 0 || cn > max(0xFF, clientlimit + maxbots))
        {
            neterr("clientnum", false);
            return nullptr;
        }
        if(cn == player1->clientnum)
        {
            return player1;
        }
        while(cn >= static_cast<int>(clients.size()))
        {
            clients.push_back(nullptr);
        }
        if(!clients[cn])
        {
            gameent *d = new gameent;
            d->clientnum = cn;
            clients[cn] = d;
            players.push_back(d);
        }
        return clients[cn];
    }

    gameent *getclient(int cn)   // ensure valid entity
    {
        if(cn == player1->clientnum)
        {
            return player1;
        }
        else if (cn < 0)
        {
            return nullptr;
        }
        else
        {
            //uint cast is no problem now, cn < 0 check above
            return (clients.size() > static_cast<uint>(cn)) ? clients.at(cn) : nullptr;
        }
    }

    void clientdisconnected(int cn, bool notify)
    {
        if(!(static_cast<int>(clients.size()) > cn))
        {
            return;
        }
        unignore(cn);
        gameent *d = clients[cn];
        if(d)
        {
            if(notify && d->name[0])
            {
                conoutf("^f4leave:^f7 %s", colorname(d));
            }
            removeweapons(d);
            removetrackedparticles(d);
            removetrackeddynlights(d);
            removegroupedplayer(d);
            auto itr = std::find(players.begin(), players.end(), d);
            if(itr != players.end())
            {
                players.erase(itr);
            }
            delete clients[cn];
            clients[cn] = nullptr;
            cleardynentcache();
        }
        if(following == cn)
        {
            if(specmode)
            {
                nextfollow();
            }
            else
            {
                stopfollowing();
            }
        }
        //we need to update the dynent vector to reflect the lost player and avoid use-after-free
        updateenginevalues();

    }

    void clearclients(bool notify)
    {
        for(uint i = 0; i < clients.size(); i++)
        {
            if(clients[i])
            {
                clientdisconnected(i, notify);
            }
        }
    }

    void initclient()
    {
        player1 = spawnstate(new gameent);
        filtertext(player1->name, "unnamed", false, false, maxnamelength);
        players.push_back(player1);
    }

    VARP(showmodeinfo, 0, 1, 1);

    void startgame()
    {
        clearprojectiles();
        clearbouncers();
        clearragdolls();
        clearteaminfo();
        // reset perma-state
        for(uint i = 0; i < players.size(); i++)
        {
            players[i]->startgame();
        }
        setclientmode();
        intermission = false;
        maptime = maprealtime = 0;
        maplimit = -1;
        conoutf(ConsoleMsg_GameInfo, "^f2game mode is %s", server::modeprettyname(gamemode));
        const char *info = validmode(gamemode) ? gamemodes[gamemode - startgamemode].info : nullptr;
        if(showmodeinfo && info)
        {
            conoutf(ConsoleMsg_GameInfo, "^f0%s", info);
        }
        syncplayer();
        showscores(false);
        disablezoom();
        lasthit = 0;
        execident("mapstart");
        execident("resethud"); //reconfigure hud
    }

    void startmap(const char *name)   // called just after a map load
    {
        ai::savewaypoints();
        ai::clearwaypoints(true);
        if(modecheck(gamemode, Mode_LocalOnly))
        {
            spawnplayer(player1);
        }
        else
        {
            findplayerspawn(player1, -1, modecheck(gamemode, Mode_Team) ? player1->team : 0);
        }
        entities::resetspawns();
        char buf[260];
        copystring(buf, name ? name : "");
        setmapname(buf);

        sendmapinfo();
    }

    const char *getmapinfo()
    {
        return showmodeinfo && validmode(gamemode) ? gamemodes[gamemode - startgamemode].info : nullptr;
    }

    void physicstrigger(physent *d, int floorlevel, int waterlevel)
    {
        if(waterlevel>0)
        {
            soundmain.playsound(Sound_SplashOut, d==player1 ? nullptr : &d->o);
        }
        else if(waterlevel<0)
        {
            soundmain.playsound(Sound_SplashIn, d==player1 ? nullptr : &d->o);
        }
        if(floorlevel>0)
        {
            if(d==player1 || d->type!=physent::PhysEnt_Player || ((gameent *)d)->ai)
            {
                msgsound(Sound_Jump, d);
            }
        }
        else if(floorlevel<0)
        {
            if(d==player1 || d->type!=physent::PhysEnt_Player || ((gameent *)d)->ai)
            {
                msgsound(Sound_Land, d);
            }
        }
    }

    void msgsound(int n, physent *d)
    {
        if(!d || d==player1)
        {
            addmsg(NetMsg_Sound, "ci", d, n);
            soundmain.playsound(n);
        }
        else
        {
            if(d->type==physent::PhysEnt_Player && ((gameent *)d)->ai)
            {
                addmsg(NetMsg_Sound, "ci", d, n);
            }
            soundmain.playsound(n, &d->o);
        }
    }

    bool duplicatename(const gameent *d, const char *name = nullptr, const char *alt = nullptr)
    {
        if(!name)
        {
            name = d->name;
        }
        if(alt && d != player1 && !strcmp(name, alt))
        {
            return true;
        }
        for(uint i = 0; i < players.size(); i++)
        {
            if(d!=players[i] && !strcmp(name, players[i]->name))
            {
                return true;
            }
        }
        return false;
    }

    const char *colorname(const gameent *d, const char *name, const char * alt, const char *color)
    {
        if(!name)
        {
            name = alt && d == player1 ? alt : d->name;
        }
        bool dup = !name[0] || duplicatename(d, name, alt) || d->aitype != AI_None;
        if(dup || color[0])
        {
            if(dup)
            {
                return tempformatstring(d->aitype == AI_None ? "^fs%s%s ^f5(%d)^fr" : "^fs%s%s ^f5[%d]^fr", color, name, d->clientnum);
            }
            return tempformatstring("^fs%s%s^fr", color, name);
        }
        return name;
    }

    VARP(teamcolortext, 0, 1, 1);

    const char *teamcolorname(const gameent *d, const char *alt)
    {
        if(!teamcolortext || modecheck(gamemode, Mode_Team) || !validteam(d->team) || d->state == ClientState_Spectator)
        {
            return colorname(d, nullptr, alt);
        }
        return colorname(d, nullptr, alt, teamtextcode[d->team]);
    }

    const char *teamcolor(const char *prefix, const char *suffix, int team, const char *alt)
    {
        if(!teamcolortext || !Mode_Team || !validteam(team))
        {
            return alt;
        }
        return tempformatstring("^fs%s%s%s%s^fr", teamtextcode[team], prefix, teamnames[team], suffix);
    }

    VARP(teamsounds, 0, 1, 1);

    void teamsound(bool sameteam, int n, const vec *loc)
    {
        soundmain.playsound(n, loc, nullptr, teamsounds ? (Mode_Team && sameteam ? Music_UseAlt : Music_NoAlt) : 0);
    }

    void teamsound(gameent *d, int n, const vec *loc)
    {
        teamsound((modecheck(gamemode, Mode_Team) && d->team == player1->team), n, loc);
    }

    void suicide(physent *d)
    {
        if(d==player1 || (d->type==physent::PhysEnt_Player && ((gameent *)d)->ai))
        {
            if(d->state!=ClientState_Alive)
            {
                return;
            }
            gameent *pl = (gameent *)d;
            if(modecheck(gamemode, Mode_LocalOnly))
            {
                killed(pl, pl);
            }
            else
            {
                int seq = (pl->lifesequence<<16)|((lastmillis/1000)&0xFFFF);
                if(pl->suicided!=seq)
                {
                    addmsg(NetMsg_Suicide, "rc", pl);
                    pl->suicided = seq;
                }
            }
        }
    }
    ICOMMAND(suicide, "", (), suicide(player1));

    void drawicon(int icon, float x, float y, float sz)
    {
        settexture("media/interface/hud/items.png");
        float tsz = 0.25f,
              tx = tsz*(icon%4),
              ty = tsz*(icon/4);
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x,    y);    gle::attribf(tx,     ty);
        gle::attribf(x+sz, y);    gle::attribf(tx+tsz, ty);
        gle::attribf(x,    y+sz); gle::attribf(tx,     ty+tsz);
        gle::attribf(x+sz, y+sz); gle::attribf(tx+tsz, ty+tsz);
        gle::end();
    }

    VARP(teamcrosshair, 0, 1, 1);
    VARP(hitcrosshair, 0, 425, 1000);

    int selectcrosshair()
    {
        gameent *d = hudplayer();
        if(d->state==ClientState_Spectator || d->state==ClientState_Dead || UI::uivisible("scoreboard"))
        {
            return -1;
        }
        if(d->state!=ClientState_Alive)
        {
            return 0;
        }
        int crosshair = 0;
        if(lasthit && lastmillis - lasthit < hitcrosshair)
        {
            crosshair = 2;
        }
        else if(teamcrosshair && Mode_Team)
        {
            dynent *o = intersectclosest(d->o, worldpos, d);
            if(o && o->type==physent::PhysEnt_Player && validteam(d->team) && (reinterpret_cast<gameent *>(o))->team == d->team)
            {
                crosshair = 1;
            }
        }
        return crosshair;
    }

    const char *mastermodecolor(int n, const char *unknown)
    {
        return (n>=MasterMode_Start && size_t(n-MasterMode_Start)<sizeof(mastermodecolors)/sizeof(mastermodecolors[0])) ? mastermodecolors[n-MasterMode_Start] : unknown;
    }

    const char *mastermodeicon(int n, const char *unknown)
    {
        return (n>=MasterMode_Start && size_t(n-MasterMode_Start)<sizeof(mastermodeicons)/sizeof(mastermodeicons[0])) ? mastermodeicons[n-MasterMode_Start] : unknown;
    }

    ICOMMAND(servinfomode, "i", (int *i), GETSERVINFOATTR(*i, 0, mode, intret(mode)));
    ICOMMAND(servinfomodename, "i", (int *i),
        GETSERVINFOATTR(*i, 0, mode,
        {
            const char *name = server::modeprettyname(mode, nullptr);
            if(name)
            {
                result(name);
            }
        }));
    ICOMMAND(servinfomastermode, "i", (int *i), GETSERVINFOATTR(*i, 2, mm, intret(mm)));
    ICOMMAND(servinfomastermodename, "i", (int *i),
        GETSERVINFOATTR(*i, 2, mm,
        {
            const char *name = server::mastermodename(mm, nullptr);
            if(name)
            {
                stringret(newconcatstring(mastermodecolor(mm, ""), name));
            }
        }));
    ICOMMAND(servinfotime, "ii", (int *i, int *raw),
        GETSERVINFOATTR(*i, 1, secs,
        {
            secs = std::clamp(secs, 0, 59*60+59);
            if(*raw)
            {
                intret(secs);
            }
            else
            {
                int mins = secs/60;
                secs %= 60;
                result(tempformatstring("%d:%02d", mins, secs));
            }
        }));
    ICOMMAND(servinfoicon, "i", (int *i),
        GETSERVINFO(*i, si,
        {
            int mm = si->attr.size() > 2 ? si->attr[2] : MasterMode_Invalid;
            result(si->maxplayers > 0 && si->numplayers >= si->maxplayers ? "serverfull" : mastermodeicon(mm, "serverunk"));
        })
    );

    const char *gameconfig()    { return "config/game.cfg"; }
    const char *defaultconfig() { return "config/default.cfg"; }
    const char *savedservers()  { return "config/servers.cfg"; }

    void loadconfigs()
    {
        execfile("config/auth.cfg", false);
    }

}
/////////////////// phys

void switchfloor(physent *d, vec &dir, const vec &floor)
{
    if(floor.z >= floorz)
    {
        d->falling = vec(0, 0, 0);
    }
    vec oldvel(d->vel);
    oldvel.add(d->falling);
    if(dir.dot(floor) >= 0)
    {
        if(d->physstate < PhysEntState_Slide || fabs(dir.dot(d->floor)) > 0.01f*dir.magnitude())
        {
            return;
        }
        d->vel.projectxy(floor, 0.0f);
    }
    else
    {
        d->vel.projectxy(floor);
    }
    d->falling.project(floor);
    recalcdir(d, oldvel, dir);
}

bool trystepup(physent *d, vec &dir, const vec &obstacle, float maxstep, const vec &floor)
{
    vec old(d->o),
        stairdir = (obstacle.z >= 0 && obstacle.z < slopez ? vec(-obstacle.x, -obstacle.y, 0) : vec(dir.x, dir.y, 0)).rescale(1);
    bool cansmooth = true;
    /* check if there is space atop the stair to move to */
    if(d->physstate != PhysEntState_StepUp)
    {
        vec checkdir = stairdir;
        checkdir.mul(0.1f);
        checkdir.z += maxstep + 0.1f;
        d->o.add(checkdir);
        if(collide(d))
        {
            d->o = old;
            if(!collide(d, nullptr, vec(0, 0, -1), slopez))
            {
                return false;
            }
            cansmooth = false;
        }
    }

    if(cansmooth)
    {
        vec checkdir = stairdir;
        checkdir.z += 1;
        checkdir.mul(maxstep);
        d->o = old;
        d->o.add(checkdir);
        int scale = 2;
        if(collide(d, nullptr, checkdir))
        {
            if(!collide(d, nullptr, vec(0, 0, -1), slopez))
            {
                d->o = old;
                return false;
            }
            d->o.add(checkdir);
            if(collide(d, nullptr, vec(0, 0, -1), slopez))
            {
                scale = 1;
            }
        }
        if(scale != 1)
        {
            d->o = old;
            d->o.sub(checkdir.mul(vec(2, 2, 1)));
            if(!collide(d, nullptr, vec(0, 0, -1), slopez))
            {
                scale = 1;
            }
        }

        d->o = old;
        vec smoothdir(dir.x, dir.y, 0);
        float magxy = smoothdir.magnitude();
        if(magxy > 1e-9f)
        {
            if(magxy > scale*dir.z)
            {
                smoothdir.mul(1/magxy);
                smoothdir.z = 1.0f/scale;
                smoothdir.mul(dir.magnitude()/smoothdir.magnitude());
            }
            else
            {
                smoothdir.z = dir.z;
            }
            d->o.add(smoothdir);
            d->o.z += maxstep + 0.1f;
            if(!collide(d, nullptr, smoothdir))
            {
                d->o.z -= maxstep + 0.1f;
                if(d->physstate == PhysEntState_Fall || d->floor != floor)
                {
                    d->timeinair = 0;
                    d->floor = floor;
                    switchfloor(d, dir, d->floor);
                }
                d->physstate = PhysEntState_StepUp;
                return true;
            }
        }
    }

    /* try stepping up */
    d->o = old;
    d->o.z += dir.magnitude();
    if(!collide(d, nullptr, vec(0, 0, 1)))
    {
        if(d->physstate == PhysEntState_Fall || d->floor != floor)
        {
            d->timeinair = 0;
            d->floor = floor;
            switchfloor(d, dir, d->floor);
        }
        if(cansmooth)
        {
            d->physstate = PhysEntState_StepUp;
        }
        return true;
    }
    d->o = old;
    return false;
}

bool trystepdown(physent *d, vec &dir, float step, float xy, float z, bool init = false)
{
    vec stepdir(dir.x, dir.y, 0);
    stepdir.z = -stepdir.magnitude2()*z/xy;
    if(!stepdir.z)
    {
        return false;
    }
    stepdir.normalize();

    vec old(d->o),
        cwall(0,0,0);
    d->o.add(vec(stepdir).mul(stairheight/fabs(stepdir.z))).z -= stairheight;
    d->zmargin = -stairheight;
    if(collide(d, &cwall, vec(0, 0, -1), slopez))
    {
        d->o = old;
        d->o.add(vec(stepdir).mul(step));
        d->zmargin = 0;
        if(!collide(d, &cwall, vec(0, 0, -1)))
        {
            vec stepfloor(stepdir);
            stepfloor.mul(-stepfloor.z).z += 1;
            stepfloor.normalize();
            if(d->physstate >= PhysEntState_Slope && d->floor != stepfloor)
            {
                // prevent alternating step-down/step-up states if player would keep bumping into the same floor
                vec stepped(d->o);
                d->o.z -= 0.5f;
                d->zmargin = -0.5f;
                if(collide(d, &cwall, stepdir) && cwall == d->floor)
                {
                    d->o = old;
                    if(!init)
                    {
                        d->o.x += dir.x;
                        d->o.y += dir.y;
                        if(dir.z <= 0 || collide(d, &cwall, dir))
                        {
                            d->o.z += dir.z;
                        }
                    }
                    d->zmargin = 0;
                    d->physstate = PhysEntState_StepDown;
                    d->timeinair = 0;
                    return true;
                }
                d->o = init ? old : stepped;
                d->zmargin = 0;
            }
            else if(init)
            {
                d->o = old;
            }
            switchfloor(d, dir, stepfloor);
            d->floor = stepfloor;
            d->physstate = PhysEntState_StepDown;
            d->timeinair = 0;
            return true;
        }
    }
    d->o = old;
    d->zmargin = 0;
    return false;
}

bool trystepdown(physent *d, vec &dir, bool init = false)
{
    if((!d->move && !d->strafe))
    {
        return false;
    }
    vec old(d->o);
    d->o.z -= stairheight;
    d->zmargin = -stairheight;
    if(!collide(d, nullptr, vec(0, 0, -1), slopez))
    {
        d->o = old;
        d->zmargin = 0;
        return false;
    }
    d->o = old;
    d->zmargin = 0;
    float step = dir.magnitude();
    // weaker check, just enough to avoid hopping up slopes
    if(trystepdown(d, dir, step, 4, 1, init))
    {
        return true;
    }
    return false;
}

void falling(physent *d, vec &dir, const vec &floor)
{
    if(floor.z > 0.0f && floor.z < slopez)
    {
        if(floor.z >= wallz)
        {
            switchfloor(d, dir, floor);
        }
        d->timeinair = 0;
        d->physstate = PhysEntState_Slide;
        d->floor = floor;
    }
    else if(d->physstate < PhysEntState_Slope || dir.dot(d->floor) > 0.01f*dir.magnitude() || (floor.z != 0.0f && floor.z != 1.0f) || !trystepdown(d, dir, true))
    {
        d->physstate = PhysEntState_Fall;
    }
}

void landing(physent *d, vec &dir, const vec &floor, bool collided)
{

    switchfloor(d, dir, floor);
    d->timeinair = 0;
    if((d->physstate!=PhysEntState_StepUp && d->physstate!=PhysEntState_StepDown) || !collided)
    {
        d->physstate = floor.z >= floorz ? PhysEntState_Floor : PhysEntState_Slope;
    }
    d->floor = floor;
}

bool findfloor(physent *d, const vec &dir, bool collided, const vec &obstacle, bool &slide, vec &floor)
{
    bool found = false;
    vec moved(d->o),
        cwall(0,0,0);
    d->o.z -= 0.1f;
    if(collide(d, &cwall, vec(0, 0, -1), d->physstate == PhysEntState_Slope || d->physstate == PhysEntState_StepDown ? slopez : floorz))
    {
        if(d->physstate == PhysEntState_StepUp && d->floor != cwall)
        {
            vec old(d->o), checkfloor(cwall), checkdir = vec(dir).projectxydir(checkfloor).rescale(dir.magnitude());
            d->o.add(checkdir);
            if(!collide(d, &cwall, checkdir))
            {
                floor = checkfloor;
                found = true;
                goto foundfloor;
            }
            d->o = old;
        }
        else
        {
            floor = cwall;
            found = true;
            goto foundfloor;
        }
    }
    if(collided && obstacle.z >= slopez)
    {
        floor = obstacle;
        found = true;
        slide = false;
    }
    else if(d->physstate == PhysEntState_StepUp || d->physstate == PhysEntState_Slide)
    {
        if(collide(d, &cwall, vec(0, 0, -1)) && cwall.z > 0.0f)
        {
            floor = cwall;
            if(floor.z >= slopez)
            {
                found = true;
            }
        }
    }
    else if(d->physstate >= PhysEntState_Slope && d->floor.z < 1.0f)
    {
        if(collide(d, &cwall, vec(d->floor).neg(), 0.95f) || collide(d, &cwall, vec(0, 0, -1)))
        {
            floor = cwall;
            if(floor.z >= slopez && floor.z < 1.0f)
            {
                found = true;
            }
        }
    }
foundfloor:
    if(collided && (!found || obstacle.z > floor.z))
    {
        floor = obstacle;
        slide = !found && (floor.z < wallz || floor.z >= slopez);
    }
    d->o = moved;
    return found;
}

bool move(physent *d, vec &dir)
{
    vec old(d->o);
    bool collided = false,
         slidecollide = false;
    vec obstacle;
    d->o.add(dir);
    vec cwall(0,0,0);
    if(collide(d, &cwall, dir))
    {
        obstacle = cwall;
        /* check to see if there is an obstacle that would prevent this one from being used as a floor (or ceiling bump) */
        if(d->type==physent::PhysEnt_Player && ((cwall.z>=slopez && dir.z<0) || (cwall.z<=-slopez && dir.z>0)) && (dir.x || dir.y) && collide(d, &cwall, vec(dir.x, dir.y, 0)))
        {
            if(cwall.dot(dir) >= 0)
            {
                slidecollide = true;
            }
            obstacle = cwall;
        }
        d->o = old;
        d->o.z -= stairheight;
        d->zmargin = -stairheight;
        if(d->physstate == PhysEntState_Slope || d->physstate == PhysEntState_Floor || (collide(d, &cwall, vec(0, 0, -1), slopez) && (d->physstate==PhysEntState_StepUp || d->physstate==PhysEntState_StepDown || cwall.z>=floorz)))
        {
            d->o = old;
            d->zmargin = 0;
            if(trystepup(d, dir, obstacle, stairheight, d->physstate == PhysEntState_Slope || d->physstate == PhysEntState_Floor ? d->floor : vec(cwall)))
            {
                return true;
            }
        }
        else
        {
            d->o = old;
            d->zmargin = 0;
        }
        /* can't step over the obstacle, so just slide against it */
        collided = true;
    }
    else if(d->physstate == PhysEntState_StepUp)
    {
        if(collide(d, &cwall, vec(0, 0, -1), slopez))
        {
            d->o = old;
            if(trystepup(d, dir, vec(0, 0, 1), stairheight, vec(cwall)))
            {
                return true;
            }
            d->o.add(dir);
        }
    }
    else if(d->physstate == PhysEntState_StepDown && dir.dot(d->floor) <= 1e-6f)
    {
        vec moved(d->o);
        d->o = old;
        if(trystepdown(d, dir))
        {
            return true;
        }
        d->o = moved;
    }
    vec floor(0, 0, 0);
    bool slide = collided,
         found = findfloor(d, dir, collided, obstacle, slide, floor);
    if(slide || (!collided && floor.z > 0 && floor.z < wallz))
    {
        slideagainst(d, dir, slide ? obstacle : floor, found, slidecollide);
        d->blocked = true;
    }
    if(found)
    {
        landing(d, dir, floor, collided);
    }
    else
    {
        falling(d, dir, floor);
    }
    return !collided;
}

void crouchplayer(physent *pl, int moveres)
{
    if(!curtime)
    {
        return;
    }
    float minheight = pl->maxheight * crouchheight, speed = (pl->maxheight - minheight) * curtime / static_cast<float>(crouchtime);
    if(pl->crouching < 0)
    {
        if(pl->eyeheight > minheight)
        {
            float diff = min(pl->eyeheight - minheight, speed);
            pl->eyeheight -= diff;
            if(pl->physstate > PhysEntState_Fall)
            {
                pl->o.z -= diff;
                pl->newpos.z -= diff;
            }
        }
    }
    else if(pl->eyeheight < pl->maxheight)
    {
        float diff = min(pl->maxheight - pl->eyeheight, speed),
              step = diff/moveres;
        pl->eyeheight += diff;
        if(pl->physstate > PhysEntState_Fall)
        {
            pl->o.z += diff;
            pl->newpos.z += diff;
        }
        pl->crouching = 0;
        for(int i = 0; i < moveres; ++i)
        {
            if(!collide(pl, nullptr, vec(0, 0, pl->physstate <= PhysEntState_Fall ? -1 : 1), 0, true))
            {
                break;
            }
            pl->crouching = 1;
            pl->eyeheight -= step;
            if(pl->physstate > PhysEntState_Fall)
            {
                pl->o.z -= step;
                pl->newpos.z -= step;
            }
        }
    }
}

static const int physframetimestd = 8;

// main physics routine, moves a player/monster for a curtime step
// moveres indicated the physics precision (which is lower for monsters and multiplayer prediction)
// local is false for multiplayer prediction

static const int inairsounddelay = 800; //time before midair players are allowed to land with a "thud"

static void handleparachute(gameent *pl, bool water)
{
    if(pl->spawnprotect && game::player1 == pl)
    {
        pl->parachutetime = lastmillis;
    }
    if(lastmillis - pl->parachutetime < parachutemaxtime
        && pl->timeinair > 0
        && !modecheck(game::gamemode, Mode_Edit)
        && !water)
    {
        pl->maxspeed = parachutespeed;
    }
    else
    {
        pl->parachutetime -= parachutemaxtime; //immediately cancel parachute
        if(pl->sprinting == 1)
        {
            pl->maxspeed = defaultspeed;
        }
        else
        {
            pl->maxspeed = 70;
        }
    }
}

bool moveplayer(gameent *pl, int moveres, bool local, int curtime) //always returns true
{
    int material = rootworld.lookupmaterial(vec(pl->o.x, pl->o.y, pl->o.z + (3*pl->aboveeye - pl->eyeheight)/4));
    bool water = IS_LIQUID(material&MatFlag_Volume);
    bool floating = pl->type==physent::PhysEnt_Player && (pl->state==ClientState_Editing || pl->state==ClientState_Spectator);
    float secs = curtime/1000.f;

    // apply gravity
    if(!floating)
    {
        modifygravity(pl, water, curtime);
    }
    // apply any player generated changes in velocity
    modifyvelocity(pl, local, water, floating, curtime);

    vec d(pl->vel);
    if(!floating && water)
    {
        d.mul(0.5f);
    }
    d.add(pl->falling);
    d.mul(secs);

    pl->blocked = false;
    handleparachute(pl, water);

    if(floating)                // just apply velocity
    {
        if(pl->physstate != PhysEntState_Float)
        {
            pl->physstate = PhysEntState_Float;
            pl->timeinair = 0;
            pl->falling = vec(0, 0, 0);
        }
        pl->o.add(d);
    }
    else                        // apply velocity with collision
    {
        const float f = 1.0f/moveres;
        const int timeinair = pl->timeinair;
        int collisions = 0;

        d.mul(f);
        for(int i = 0; i < moveres; ++i)
        {
            if(!move(pl, d) && ++collisions<5)
            {
                i--; // discrete steps collision detection & sliding
                pl->sprinting = 1; //if collided, turn off sprint
            }
        }
        if(timeinair > inairsounddelay && !pl->timeinair && !water) // if we land after long time must have been a high jump, make thud sound
        {
            game::physicstrigger(pl, -1, 0);
        }
    }
    if(std::abs(pl->vel.x) < 1 && std::abs(pl->vel.y) < 1 && std::abs(pl->vel.z) < 1)
    {
        pl->sprinting = 1;
    }
    if(pl->state==ClientState_Alive)
    {
        updatedynentcache(pl);
    }
    // play sounds on water transitions
    if(pl->inwater && !water)
    {
        material = rootworld.lookupmaterial(vec(pl->o.x, pl->o.y, pl->o.z + (pl->aboveeye - pl->eyeheight)/2));
        water = IS_LIQUID(material&MatFlag_Volume);
    }
    if(!pl->inwater && water)
    {
        game::physicstrigger(pl, 0, -1);
    }
    else if(pl->inwater && !water)
    {
        game::physicstrigger(pl, 0, 1);
    }
    pl->inwater = water ? material&MatFlag_Volume : Mat_Air;
    //tell players who enter deatmat who are alive to kill themselves
    if(pl->state==ClientState_Alive && (pl->o.z < 0 || material&Mat_Death))
    {
        game::suicide(pl);
    }
    return true;
}


void modifyvelocity(physent *pl, bool local, bool water, bool floating, int curtime)
{
    if(floating)
    {
        if(pl->jumping)
        {
            pl->jumping = false;
            pl->vel.z = max(pl->vel.z, jumpvel);
        }
    }
    else if(pl->physstate >= PhysEntState_Slope || water)
    {
        if(water && !pl->inwater)
        {
            pl->vel.div(8);
        }
        if(pl->jumping)
        {
            pl->jumping = false;
            pl->vel.z = max(pl->vel.z, jumpvel); // physics impulse upwards
            if(water)
            {
                pl->vel.x /= 8.0f;
                pl->vel.y /= 8.0f;
            } // dampen velocity change even harder, gives correct water feel
            game::physicstrigger(pl, 1, 0);
        }
    }
    if(!floating && pl->physstate == PhysEntState_Fall)
    {
        pl->timeinair += curtime;
    }
    vec m(0.0f, 0.0f, 0.0f);
    if(pl->move || pl->strafe)
    {
        vecfromyawpitch(pl->yaw, floating || water || pl->type==physent::PhysEnt_Camera ? pl->pitch : 0, pl->move, pl->strafe, m);
        if(!floating && pl->physstate >= PhysEntState_Slope)
        {
            /* move up or down slopes in air
             * but only move up slopes in water
             */
            float dz = -(m.x*pl->floor.x + m.y*pl->floor.y)/pl->floor.z;
            m.z = water ? max(m.z, dz) : dz;
        }
        m.normalize();
    }

    vec d(m);
    d.mul(pl->maxspeed);
    if(pl->type==physent::PhysEnt_Player)
    {
        if(floating)
        {
            if(pl==player)
            {
                d.mul(floatspeed/100.0f);
            }
        }
        else if(pl->crouching)
        {
            d.mul(0.4f);
        }
    }
    float fric = water && !floating ? 20.0f : (pl->physstate >= PhysEntState_Slope || floating ? 6.0f : 30.0f);
    pl->vel.lerp(d, pl->vel, pow(1 - 1/fric, curtime/20.0f));
// old fps friction
//    float friction = water && !floating ? 20.0f : (pl->physstate >= PhysEntState_Slope || floating ? 6.0f : 30.0f);
//    float fpsfric = min(curtime/(20.0f*friction), 1.0f);
//    pl->vel.lerp(pl->vel, d, fpsfric);
}

void modifygravity(gameent *pl, bool water, int curtime)
{
    float secs = curtime/1000.0f;
    vec g(0, 0, 0);
    if(pl->physstate == PhysEntState_Fall)
    {
        //freeze newly spawned players until they move
        if(pl->spawnprotect)
        {
            g.z -= 0.0f;
        }
        //parachute
        else if(lastmillis - pl->parachutetime < parachutemaxtime)
        {
            g.z -= 0.05;
        }
        //normal fall
        else
        {
            g.z -= gravity*secs;
        }
    }
    else if(pl->floor.z > 0 && pl->floor.z < floorz)
    {
        g.z = -1;
        g.project(pl->floor);
        g.normalize();
        g.mul(gravity*secs);
    }
    if(!water || (!pl->move && !pl->strafe))
    {
        pl->falling.add(g);
    }
    if(water || pl->physstate >= PhysEntState_Slope)
    {
        float fric = water ? 2.0f : 6.0f,
              c = water ? 1.0f : std::clamp((pl->floor.z - slopez)/(floorz-slopez), 0.0f, 1.0f);
        pl->falling.mul(pow(1 - c/fric, curtime/20.0f));
// old fps friction
//        float friction = water ? 2.0f : 6.0f,
//              fpsfric = friction/curtime*20.0f,
//              c = water ? 1.0f : std::clamp((pl->floor.z - slopez)/(floorz-slopez), 0.0f, 1.0f);
//        pl->falling.mul(1 - c/fpsfric);
    }
}

int physsteps = 0,
    physframetime = physframetimestd,
    lastphysframe = 0;

void physicsframe()          // optimally schedule physics frames inside the graphics frames
{
    int diff = lastmillis - lastphysframe;
    if(diff <= 0)
    {
        physsteps = 0;
    }
    else
    {
        physframetime = std::clamp(game::scaletime(physframetimestd)/100, 1, physframetimestd);
        physsteps = (diff + physframetime - 1)/physframetime;
        lastphysframe += physsteps * physframetime;
    }
    cleardynentcache();
}

VAR(physinterp, 0, 1, 1);

void interppos(physent *pl)
{
    pl->o = pl->newpos;

    int diff = lastphysframe - lastmillis;
    if(diff <= 0 || !physinterp)
    {
        return;
    }
    vec deltapos(pl->deltapos);
    deltapos.mul(min(diff, physframetime)/static_cast<float>(physframetime));
    pl->o.add(deltapos);
}

void moveplayer(gameent *pl, int moveres, bool local)
{
    if(physsteps <= 0)
    {
        if(local)
        {
            interppos(pl);
        }
        return;
    }
    if(local)
    {
        pl->o = pl->newpos;
    }
    for(int i = 0; i < physsteps-1; ++i)
    {
        moveplayer(pl, moveres, local, physframetime);
    }
    if(local)
    {
        pl->deltapos = pl->o;
    }
    moveplayer(pl, moveres, local, physframetime);
    if(local)
    {
        pl->newpos = pl->o;
        pl->deltapos.sub(pl->newpos);
        interppos(pl);
    }
}

void updatephysstate(physent *d)
{
    if(d->physstate == PhysEntState_Fall)
    {
        return;
    }
    d->timeinair = 0;
    vec old(d->o),
        cwall(0,0,0);
    /* Attempt to reconstruct the floor state.
     * May be inaccurate since movement collisions are not considered.
     * If good floor is not found, just keep the old floor and hope it's correct enough.
     */
    switch(d->physstate)
    {
        case PhysEntState_Slope:
        case PhysEntState_Floor:
        case PhysEntState_StepDown:
            d->o.z -= 0.15f;
            if(collide(d, &cwall, vec(0, 0, -1), d->physstate == PhysEntState_Slope || d->physstate == PhysEntState_StepDown ? slopez : floorz))
            {
                d->floor = cwall;
            }
            break;

        case PhysEntState_StepUp:
            d->o.z -= stairheight+0.15f;
            if(collide(d, &cwall, vec(0, 0, -1), slopez))
            {
                d->floor = cwall;
            }
            break;

        case PhysEntState_Slide:
            d->o.z -= 0.15f;
            if(collide(d, &cwall, vec(0, 0, -1)) && cwall.z < slopez)
            {
                d->floor = cwall;
            }
            break;
    }
    if(d->physstate > PhysEntState_Fall && d->floor.z <= 0)
    {
        d->floor = vec(0, 0, 1);
    }
    d->o = old;
}

// "convenience" macro to define movement directions
/*creates a normal inline command but takes the following arguments:
 * name: name of the movement command
 * v: velocity type (move: along camera axis; strafe: side-to-side perp from cam)
 * d: direction: positive (forward, left) or negative (backwards, right) along axes
 * s: movement key
 * os: opposite movement key (can't do both of these @ same time)
 * sprint: allows sprinting in this direction
 */

#define DIR(name,v,d,s,os, sprint) ICOMMAND(name, "D", (const int *down), \
{ \
    game::player1->s = *down!=0; \
    game::player1->v = player->s ? d : (player->os ? -(d) : 0); \
    game::player1->spawnprotect = false; \
    if(!sprint) \
    { \
        game::player1->sprinting = 1; \
    } \
}); \

DIR(backward, move,   -1, k_down,  k_up,   false);
DIR(forward,  move,    1, k_up,    k_down, true);
DIR(left,     strafe,  1, k_left,  k_right,true);
DIR(right,    strafe, -1, k_right, k_left, true);

#undef DIR

//special movement actions
ICOMMAND(jump,   "D", (int *down), { if(!*down || game::canjump()) player->jumping = *down!=0; });
ICOMMAND(crouch, "D", (int *down), { if(!*down) player->crouching = abs(player->crouching); else if(game::cancrouch()) player->crouching = -1; });
ICOMMAND(sprint, "D", (int *down), {
    if(game::cansprint() && *down)
    {
        game::player1->sprinting = -1;
    }
});
////////////////////////// camera /////////////////////////


VAR(thirdperson, 0, 0, 2);
FVAR(thirdpersondistance, 0, 30, 50);
FVAR(thirdpersonup, -25, 0, 25);
FVAR(thirdpersonside, -25, 0, 25);

void recomputecamera()
{
    game::setupcamera();
    computezoom();

    bool allowthirdperson = game::allowthirdperson();
    bool shoulddetach = (allowthirdperson && thirdperson > 1) || game::detachcamera();
    if((!allowthirdperson || !thirdperson) && !shoulddetach)
    {
        camera1 = player;
        detachedcamera = false;
    }
    else
    {
        static physent tempcamera;
        camera1 = &tempcamera;
        if(detachedcamera && shoulddetach)
        {
            camera1->o = player->o;
        }
        else
        {
            *camera1 = *player;
            detachedcamera = shoulddetach;
        }
        camera1->reset();
        camera1->type = physent::PhysEnt_Camera;
        camera1->move = -1;
        camera1->eyeheight = camera1->aboveeye = camera1->radius = camera1->xradius = camera1->yradius = 2;

        matrix3 orient;
        orient.identity();
        orient.rotate_around_z(camera1->yaw/RAD);
        orient.rotate_around_x(camera1->pitch/RAD);
        orient.rotate_around_y(camera1->roll/RAD);
        vec dir = vec(orient.b).neg(),
            side = vec(orient.a).neg(), up = orient.c;

        if(game::collidecamera())
        {
            movecamera(camera1, dir, thirdpersondistance, 1);
            movecamera(camera1, dir, std::clamp(thirdpersondistance - camera1->o.dist(player->o), 0.0f, 1.0f), 0.1f);
            if(thirdpersonup)
            {
                vec pos = camera1->o;
                float dist = fabs(thirdpersonup);
                if(thirdpersonup < 0)
                {
                    up.neg();
                }
                movecamera(camera1, up, dist, 1);
                movecamera(camera1, up, std::clamp(dist - camera1->o.dist(pos), 0.0f, 1.0f), 0.1f);
            }
            if(thirdpersonside)
            {
                vec pos = camera1->o;
                float dist = fabs(thirdpersonside);
                if(thirdpersonside < 0)
                {
                    side.neg();
                }
                movecamera(camera1, side, dist, 1);
                movecamera(camera1, side, std::clamp(dist - camera1->o.dist(pos), 0.0f, 1.0f), 0.1f);
            }
        }
        else
        {
            camera1->o.add(vec(dir).mul(thirdpersondistance));
            if(thirdpersonup)
            {
                camera1->o.add(vec(up).mul(thirdpersonup));
            }
            if(thirdpersonside)
            {
                camera1->o.add(vec(side).mul(thirdpersonside));
            }
        }
    }
}
