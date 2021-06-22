#include "game.h"

//player model rendering (both hud player and 3p player rendering)
//for "real" rendering, see /src/engine/render*
//includes:
//hud player rendering
//3p player rendering
//ragdoll handling
//player colors


namespace game
{
    vector<gameent *> bestplayers;
    vector<int> bestteams;

    VARP(ragdoll, 0, 1, 1);                 //enables ragdolls
    VARP(ragdollmillis, 0, 10000, 300000);  //ragdoll lifetime
    VARP(ragdollfade, 0, 100, 5000);        //ragdoll fade time
    VARP(forceplayermodels, 0, 0, 1);       //force default player model
    VARP(showdead, 0, 1, 1);                //show dead bodies

    extern int playermodel;

    vector<gameent *> ragdolls;

    void saveragdoll(gameent *d)
    {
        if(!d->ragdoll || !ragdollmillis || (!ragdollfade && lastmillis > d->lastpain + ragdollmillis))
        {
            return;
        }
        gameent *r = new gameent(*d);
        r->lastupdate = ragdollfade && lastmillis > d->lastpain + max(ragdollmillis - ragdollfade, 0) ? lastmillis - max(ragdollmillis - ragdollfade, 0) : d->lastpain;
        r->edit = NULL;
        r->ai = NULL;
        if(d==player1)
        {
            r->playermodel = playermodel;
        }
        ragdolls.add(r);
        d->ragdoll = NULL;
    }

    void clearragdolls()
    {
        ragdolls.deletecontents();
    }

    void moveragdolls()
    {
        for(int i = 0; i < ragdolls.length(); i++)
        {
            gameent *d = ragdolls[i];
            if(lastmillis > d->lastupdate + ragdollmillis)
            {
                delete ragdolls.remove(i--);
                continue;
            }
            moveragdoll(d);
        }
    }
    //ffa colors
    static const int playercolors[] =
    {
        0xA12020,
        0xA15B28,
        0xB39D52,
        0x3E752F,
        0x3F748C,
        0x214C85,
        0xB3668C,
        0x523678,
        0xB3ADA3
    };
    //azul (blue) team allowed colors
    static const int playercolorsazul[] =
    {
        0x27508A,
        0x3F748C,
        0x3B3B80,
        0x5364B5
    };
    //rojo (red) team allowed colors
    static const int playercolorsrojo[] =
    {
        0xAC2C2A,
        0x992417,
        0x802438,
        0xA3435B
    };

    extern void changedplayercolor();
    VARFP(playercolor, 0, 4, sizeof(playercolors)/sizeof(playercolors[0])-1, changedplayercolor());
    VARFP(playercolorazul, 0, 0, sizeof(playercolorsazul)/sizeof(playercolorsazul[0])-1, changedplayercolor());
    VARFP(playercolorrojo, 0, 0, sizeof(playercolorsrojo)/sizeof(playercolorsrojo[0])-1, changedplayercolor());

    static const playermodelinfo playermodels[] =
    {
        {
            { "player/bones", "player/bones", "player/bones" },
            { "hudgun", "hudgun", "hudgun" },
            { "player", "player_azul", "player_rojo" },
            true
        }
    };

    static const char * parachutemodel = "parachute";

    extern void changedplayermodel();
    VARFP(playermodel, 0, 0, sizeof(playermodels)/sizeof(playermodels[0])-1, changedplayermodel());

    int chooserandomplayermodel(int seed)
    {
        return (seed&0xFFFF)%(sizeof(playermodels)/sizeof(playermodels[0]));
    }

    const playermodelinfo *getplayermodelinfo(int n)
    {
        if(size_t(n) >= sizeof(playermodels)/sizeof(playermodels[0]))
        {
            return NULL;
        }
        return &playermodels[n];
    }

    const playermodelinfo &getplayermodelinfo(gameent *d)
    {
        const playermodelinfo *mdl = getplayermodelinfo(d==player1 || forceplayermodels ? playermodel : d->playermodel);
        if(!mdl)
        {
            mdl = getplayermodelinfo(playermodel);
        }
        return *mdl;
    }

    int getplayercolor(int team, int color)
    {
        #define GETPLAYERCOLOR(playercolors) \
            return playercolors[color%(sizeof(playercolors)/sizeof(playercolors[0]))];
        switch(team)
        {
            case 1:
            {
                GETPLAYERCOLOR(playercolorsazul)
            }
            case 2:
            {
                GETPLAYERCOLOR(playercolorsrojo)
            }
            default:
            {
                GETPLAYERCOLOR(playercolors)
            }
        }
    }

    //returns color of a given player given a team (which they may or may not be on)
    ICOMMAND(getplayercolor, "ii", (int *color, int *team), intret(getplayercolor(*team, *color)));

    int getplayercolor(gameent *d, int team)
    {
        if(d==player1) switch(team)
        {
            case 1:
            {
                return getplayercolor(1, playercolorazul);
            }
            case 2:
            {
                return getplayercolor(2, playercolorrojo);
            }
            default:
            {
                return getplayercolor(0, playercolor);
            }
        }
        else
        {
            return getplayercolor(team, (d->playercolor>>(5*team))&0x1F);
        }
    }

    void changedplayermodel()
    {
        if(player1->clientnum < 0)
        {
            player1->playermodel = playermodel;
        }
        if(player1->ragdoll)
        {
            cleanragdoll(player1);
        }
        for(int i = 0; i < ragdolls.length(); i++)
        {
            gameent *d = ragdolls[i];
            if(!d->ragdoll)
            {
                continue;
            }
            if(!forceplayermodels)
            {
                const playermodelinfo *mdl = getplayermodelinfo(d->playermodel);
                if(mdl)
                {
                    continue;
                }
            }
            cleanragdoll(d);
        }
        for(int i = 0; i < players.length(); i++)
        {
            gameent *d = players[i];
            if(d == player1 || !d->ragdoll)
            {
                continue;
            }
            if(!forceplayermodels)
            {
                const playermodelinfo *mdl = getplayermodelinfo(d->playermodel);
                if(mdl)
                {
                    continue;
                }
            }
            cleanragdoll(d);
        }
    }

    void changedplayercolor()
    {
        if(player1->clientnum < 0)
        {
            player1->playercolor = playercolor | (playercolorazul<<5) | (playercolorrojo<<10);
        }
    }

    void syncplayer()
    {
        if(player1->playermodel != playermodel)
        {
            player1->playermodel = playermodel;
            addmsg(NetMsg_SwitchModel, "ri", player1->playermodel);
        }
        int col = playercolor | (playercolorazul<<5) | (playercolorrojo<<10);
        if(player1->playercolor != col)
        {
            player1->playercolor = col;
            addmsg(NetMsg_SwitchColor, "ri", player1->playercolor);
        }
    }

    void preloadplayermodel()
    {
        for(int i = 0; i < static_cast<int>(sizeof(playermodels)/sizeof(playermodels[0])); ++i)
        {
            const playermodelinfo *mdl = getplayermodelinfo(i);
            if(!mdl) //don't preload a model that isn't there
            {
                break;
            }
            if(i != playermodel && (!multiplayer || forceplayermodels))
            {
                continue;
            }
            if(modecheck(gamemode, Mode_Team))
            {
                for(int j = 0; j < maxteams; ++j)
                {
                    preloadmodel(mdl->model[1+j]);
                }
            }
            else
            {
                preloadmodel(mdl->model[0]);
            }
            preloadmodel(parachutemodel);
        }
    }

    //============================================ 3p/other player rendering =======================//

    VAR(animoverride, -1, 0, Anim_NumAnims-1); //overrides player models onscreen with selected anim index
    VAR(testanims, 0, 0, 1);    //fixes yaw to zero
    VAR(testpitch, -90, 0, 90); // fixes anim pitch to given value

    void renderplayer(gameent *d, const playermodelinfo &mdl, int color, int team, float fade, int flags = 0, bool mainpass = true)
    {
        int lastaction = d->lastaction, anim = Anim_Idle | Anim_Loop, attack = 0, delay = 0;
        if(d->lastattack >= 0)
        {
            attack = attacks[d->lastattack].anim;
            delay = attacks[d->lastattack].attackdelay+50;
        }
        if(intermission && d->state!=ClientState_Dead)
        {
            anim = attack = Anim_Lose | Anim_Loop;
            if(validteam(team) ? bestteams.htfind(team)>=0 : bestplayers.find(d)>=0)
            {
                anim = attack = Anim_Win | Anim_Loop;
            }
        }
        modelattach a[5];
        int ai = 0;
        if(guns[d->gunselect].vwep)
        {
            int vanim = Anim_VWepIdle | Anim_Loop, vtime = 0;
            if(lastaction && d->lastattack >= 0 && attacks[d->lastattack].gun==d->gunselect && lastmillis < lastaction + delay)
            {
                vanim = attacks[d->lastattack].vwepanim;
                vtime = lastaction;
            }
            a[ai++] = modelattach("tag_weapon", guns[d->gunselect].vwep, vanim, vtime);
        }
        if(mainpass && !(flags&Model_OnlyShadow))
        {
            d->muzzle = vec(-1, -1, -1);
            if(guns[d->gunselect].vwep)
            {
                a[ai++] = modelattach("tag_muzzle", &d->muzzle);
            }
        }
        const char *mdlname = mdl.model[validteam(team) ? team : 0];
        float yaw = testanims && d==player1 ? 0 : d->yaw,
              pitch = testpitch && d==player1 ? testpitch : d->pitch;
        vec o = d->feetpos();
        int basetime = 0;
        if(animoverride)
        {
            anim = (animoverride<0 ? Anim_All : animoverride) | Anim_Loop;
        }
        else if(d->state==ClientState_Dead)
        {
            anim = Anim_Dying | Anim_NoPitch;
            basetime = d->lastpain;
            if(ragdoll && mdl.ragdoll)
            {
                anim |= Anim_Ragdoll;
            }
            else if(lastmillis-basetime>1000)
            {
                anim = Anim_Dead | Anim_Loop | Anim_NoPitch;
            }
        }
        else if(d->state==ClientState_Editing || d->state==ClientState_Spectator)
        {
            anim = Anim_Edit | Anim_Loop;
        }
        else if(d->state==ClientState_Lagged)
        {
            anim = Anim_Lag | Anim_Loop;
        }
        else if(!intermission)
        {
            if(lastmillis-d->lastpain < 300)
            {
                anim = Anim_Pain;
                basetime = d->lastpain;
            }
            else if(d->lastpain < lastaction && lastmillis-lastaction < delay)
            {
                anim = attack;
                basetime = lastaction;
            }

            if(d->inwater && d->physstate<=PhysEntState_Fall)
            {
                anim |= ((d->move || d->strafe || d->vel.z+d->falling.z>0 ? Anim_Swim : Anim_Sink) | Anim_Loop) << Anim_Secondary;
            }
            else
            {
                static const int dirs[9] =
                {
                    Anim_RunSE, Anim_RunS, Anim_RunSW,
                    Anim_RunE,  0,          Anim_RunSE,
                    Anim_RunNE, Anim_RunN, Anim_RunNW
                };
                int dir = dirs[(d->move+1)*3 + (d->strafe+1)];
                if(d->timeinair>100)
                {
                    anim |= ((dir ? dir+Anim_JumpN-Anim_RunN : Anim_Jump) | Anim_End) << Anim_Secondary;
                }
                else if(dir)
                {
                    anim |= (dir | Anim_Loop) << Anim_Secondary;
                }
            }
            if(d->crouching)
            {
                switch((anim >> Anim_Secondary) & Anim_Index)
                {
                    case Anim_Idle:
                    {
                        anim &= ~(Anim_Index << Anim_Secondary);
                        anim |= Anim_Crouch << Anim_Secondary;
                        break;
                    }
                    case Anim_Jump:
                    {
                        anim &= ~(Anim_Index << Anim_Secondary);
                        anim |= Anim_CrouchJump << Anim_Secondary;
                        break;
                    }
                    case Anim_Swim:
                    {
                        anim &= ~(Anim_Index << Anim_Secondary);
                        anim |= Anim_CrouchSwim << Anim_Secondary;
                        break;
                    }
                    case Anim_Sink:
                    {
                        anim &= ~(Anim_Index << Anim_Secondary);
                        anim |= Anim_CrouchSink << Anim_Secondary;
                        break;
                    }
                    case 0:
                    {
                        anim |= (Anim_Crouch | Anim_Loop) << Anim_Secondary;
                        break;
                    }
                    case Anim_RunN:
                    case Anim_RunNE:
                    case Anim_RunE:
                    case Anim_RunSE:
                    case Anim_RunS:
                    case Anim_RunSW:
                    case Anim_RunW:
                    case Anim_RunNW:
                    {
                        anim += (Anim_CrouchN - Anim_RunN) << Anim_Secondary;
                        break;
                    }
                    case Anim_JumpN:
                    case Anim_JumpNE:
                    case Anim_JumpE:
                    case Anim_JumpSE:
                    case Anim_JumpS:
                    case Anim_JumpSW:
                    case Anim_JumpW:
                    case Anim_JumpNW:
                    {
                        anim += (Anim_CrouchJumpN - Anim_JumpN) << Anim_Secondary;
                        break;
                    }
                }
            }
            if((anim & Anim_Index) == Anim_Idle && (anim >> Anim_Secondary) & Anim_Index)
            {
                anim >>= Anim_Secondary;
            }
        }
        if(!((anim >> Anim_Secondary) & Anim_Index))
        {
            anim |= (Anim_Idle | Anim_Loop) << Anim_Secondary;
        }
        if(d!=player1)
        {
            flags |= Model_CullVFC | Model_CullOccluded | Model_CullQuery;
        }
        if(d->type==PhysEnt_Player)
        {
            flags |= Model_FullBright;
        }
        else
        {
            flags |= Model_CullDist;
        }
        if(!mainpass)
        {
            flags &= ~(Model_FullBright | Model_CullVFC | Model_CullOccluded | Model_CullQuery | Model_CullDist);
        }
        float trans = d->state == ClientState_Lagged ? 0.5f : 1.0f;
        rendermodel(mdlname, anim, o, yaw, pitch, 0, flags, d, a[0].tag ? a : NULL, basetime, 0, fade, vec4(vec::hexcolor(color), trans));
    }

    static inline void renderplayer(gameent *d, float fade = 1, int flags = 0)
    {
        int team = modecheck(gamemode, Mode_Team) && validteam(d->team) ? d->team : 0;
        renderplayer(d, getplayermodelinfo(d), getplayercolor(d, team), team, fade, flags);
    }

    static void renderparachute(gameent *d)
    {
        vec loc = vec(0,0,24).add(d->o); //three meters above player
        rendermodel(parachutemodel, Anim_Mapmodel | Anim_Loop, loc, atan2(d->vel.y,d->vel.x)/RAD, 0, 0);
    }

    void renderengineercursor()
    {
        if(player1->gunselect != Gun_Eng)
        {
            return; //don't render block guide for non block placing weapons
        }
        int d   = DIMENSION(sel.orient),
            od  = DIMENSION(orient);

        float sdist = 0,
              wdist = 0,
              t;
        int entorient = 0,
            ent = -1;

        wdist = rayent(player->o, camdir, 1e16f,
                       (editmode && showmat ? Ray_EditMat : 0)   // select cubes first
                       | (!dragging && entediting ? Ray_Ents : 0)
                       | Ray_SkipFirst
                       | (passthroughcube==1 ? Ray_Pass : 0), gridsize, entorient, ent);

        if(wdist >= attacks[Attack_EngShoot].projspeed*attacks[Attack_EngShoot].time)
        {
            return; //upon checking range, if it is larger than the eng's max placement distance, don't bother rendering
        }

        vec w = vec(camdir).mul(wdist+0.05f).add(player->o);
        cube *c = &lookupcube(ivec(w));
        gridsize = 8;
        int mag = lusize / gridsize;
        normalizelookupcube(ivec(w));
        if(sdist == 0 || sdist > wdist)
        {
            rayboxintersect(vec(lu), vec(gridsize), player->o, camdir, t=0, orient); // just getting orient
        }
        cur = lu;
        cor = ivec(vec(w).mul(2).div(gridsize));
        od = DIMENSION(orient);
        d = DIMENSION(sel.orient);

        sel.o = lu;
        sel.s.x = sel.s.y = sel.s.z = 1;
        sel.cx = sel.cy = 0;
        sel.cxs = sel.cys = 2;
        sel.grid = gridsize;
        sel.orient = orient;
        d = od;
        sel.corner = (cor[R[d]]-(lu[R[d]]*2)/gridsize)+(cor[C[d]]-(lu[C[d]]*2)/gridsize)*2;
        selchildcount = 0;
        selchildmat = -1;
        countselchild(worldroot, ivec(0, 0, 0), worldsize/2);
        if(mag>=1 && selchildcount==1)
        {
            selchildmat = c->material;
            if(mag>1)
            {
                selchildcount = -mag;
            }
        }
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        // cursors

        ldrnotextureshader->set();

        boxoutline = outline!=0;

        enablepolygonoffset(GL_POLYGON_OFFSET_LINE);

        if(iscubesolid(*c) || checkcubefill(*c))
        {
            lu.add(ivec(0,0,8));
        }
        gle::colorub(190,190,190);

        boxs3D(vec(lu), vec(lusize), 1);

        disablepolygonoffset(GL_POLYGON_OFFSET_LINE);

        boxoutline = false;

        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
    }

    void rendergame()
    {
        ai::render();
        if(intermission)
        {
            bestteams.shrink(0);
            bestplayers.shrink(0);
            if(modecheck(gamemode, Mode_Team))
            {
                getbestteams(bestteams);
            }
            else
            {
                getbestplayers(bestplayers);
            }
        }

        bool third = isthirdperson();
        gameent *f = followingplayer(),
                *exclude = third ? NULL : f;
        for(int i = 0; i < players.length(); i++)
        {
            gameent *d = players[i];
            if(   d == player1
               || d->state==ClientState_Spectator
               || d->state==ClientState_Spawning
               || d->lifesequence < 0
               || d == exclude
               || (d->state==ClientState_Dead && !showdead))
            {
                continue;
            }
            renderplayer(d);
            copystring(d->info, colorname(d));
            if(d->state!=ClientState_Dead)
            {
                int team = modecheck(gamemode, Mode_Team) && validteam(d->team) ? d->team : 0;
                particle_text(d->abovehead(), d->info, Part_Text, 1, teamtextcolor[team], 2.0f);
            }
            if(lastmillis - d->parachutetime < parachutemaxtime && d->timeinair > 0 && !modecheck(game::gamemode, Mode_Edit))
            {
                renderparachute(d);
            }
        }
        for(int i = 0; i < ragdolls.length(); i++)
        {
            gameent *d = ragdolls[i];
            float fade = 1.0f;
            if(ragdollmillis && ragdollfade)
            {
                fade -= std::clamp(static_cast<float>(lastmillis - (d->lastupdate + max(ragdollmillis - ragdollfade, 0)))/min(ragdollmillis, ragdollfade), 0.0f, 1.0f);
            }
            renderplayer(d, fade);
        }
        if(exclude)
        {
            renderplayer(exclude, 1, Model_OnlyShadow);
        }
        else if(!f && (player1->state==ClientState_Alive || (player1->state==ClientState_Editing && third) || (player1->state==ClientState_Dead && showdead)))
        {
            renderplayer(player1, 1, third ? 0 : Model_OnlyShadow);
            if(lastmillis - player1->parachutetime < parachutemaxtime && player1->timeinair > 0 && !modecheck(game::gamemode, Mode_Edit))
            {
                renderparachute(player1);
            }
        }
        renderbouncers();
        if(cmode)
        {
            cmode->rendergame();
        }
        renderengineercursor();
    }

    //============================================ hud player rendering ============================//

    VARP(hudgun, 0, 1, 1);     // toggles display of player's own gun
    VARP(hudgunsway, 0, 1, 1); // toggles sway back and forth with stride

    FVAR(swaystep, 1, 35.0f, 100); // time to sway back and forth (larger = slower)
    FVAR(swayside, 0, 0.10f, 1);   // side to side sway distance scale
    FVAR(swayup, -1, 0.15f, 1);    // up and down sway distance scale

    float swayfade = 0,
          swayspeed = 0,
          swaydist = 0;
    vec swaydir(0, 0, 0);

    void swayhudgun(int curtime)
    {
        gameent *d = hudplayer();
        if(d->state != ClientState_Spectator)
        {
            if(d->physstate >= PhysEntState_Slope)
            {
                swayspeed = min(sqrtf(d->vel.x*d->vel.x + d->vel.y*d->vel.y), d->maxspeed);
                swaydist += swayspeed*curtime/1000.0f;
                swaydist = fmod(swaydist, 2*swaystep);
                swayfade = 1;
            }
            else if(swayfade > 0)
            {
                swaydist += swayspeed*swayfade*curtime/1000.0f;
                swaydist = fmod(swaydist, 2*swaystep);
                swayfade -= 0.5f*(curtime*d->maxspeed)/(swaystep*1000.0f);
            }

            float k = pow(0.7f, curtime/10.0f);
            swaydir.mul(k);
            vec vel(d->vel);
            vel.add(d->falling);
            swaydir.add(vec(vel).mul((1-k)/(15*max(vel.magnitude(), d->maxspeed))));
        }
    }

    struct hudent : dynent
    {
        hudent() { type = PhysEnt_Camera; }
    } guninterp;

    void drawhudmodel(gameent *d, int anim, int basetime)
    {
        const char *file = guns[d->gunselect].file;
        if(!file)
        {
            return;
        }
        vec sway;
        vecfromyawpitch(d->yaw, 0, 0, 1, sway);
        float steps = swaydist/swaystep*M_PI;
        sway.mul(swayside*cosf(steps));
        sway.z = swayup*(fabs(sinf(steps)) - 1);
        sway.add(swaydir).add(d->o);
        if(!hudgunsway)
        {
            sway = d->o;
        }
        const playermodelinfo &mdl = getplayermodelinfo(d);
        int team = modecheck(gamemode, Mode_Team) && validteam(d->team) ? d->team : 0,
            color = getplayercolor(d, team);
        DEF_FORMAT_STRING(gunname, "%s/%s", mdl.hudguns[team], file);
        modelattach a[2];
        d->muzzle = vec(-1, -1, -1);
        a[0] = modelattach("tag_muzzle", &d->muzzle);
        rendermodel(gunname, anim, sway, d->yaw, d->pitch, 0, Model_NoBatch, NULL, a, basetime, 0, 1, vec4(vec::hexcolor(color), 1));
        if(d->muzzle.x >= 0)
        {
            d->muzzle = calcavatarpos(d->muzzle, 12);
        }
    }

    void drawhudgun()
    {
        gameent *d = hudplayer();
        if(d->state==ClientState_Spectator || d->state==ClientState_Editing || !hudgun || editmode)
        {
            d->muzzle = player1->muzzle = vec(-1, -1, -1);
            return;
        }

        int anim = Anim_GunIdle | Anim_Loop, basetime = 0;
        if(d->lastaction && d->lastattack >= 0 && attacks[d->lastattack].gun==d->gunselect && lastmillis-d->lastaction<attacks[d->lastattack].attackdelay)
        {
            anim = attacks[d->lastattack].hudanim;
            basetime = d->lastaction;
        }
        drawhudmodel(d, anim, basetime);
    }

    void renderavatar()
    {
        drawhudgun();
    }

    vec hudgunorigin(int gun, const vec &from, const vec &to, gameent *d)
    {
        if(d->muzzle.x >= 0)
        {
            return d->muzzle;
        }
        vec offset(from);
        if(d!=hudplayer() || isthirdperson())
        {
            vec front, right;
            vecfromyawpitch(d->yaw, d->pitch, 1, 0, front);
            offset.add(front.mul(d->radius));
            offset.z += (d->aboveeye + d->eyeheight)*0.75f - d->eyeheight;
            vecfromyawpitch(d->yaw, 0, 0, -1, right);
            offset.add(right.mul(0.5f*d->radius));
            offset.add(front);
            return offset;
        }
        offset.add(vec(to).sub(from).normalize().mul(2));
        if(hudgun)
        {
            offset.sub(vec(camup).mul(1.0f));
            offset.add(vec(camright).mul(0.8f));
        }
        else
        {
            offset.sub(vec(camup).mul(0.8f));
        }
        return offset;
    }

    void preloadweapons()
    {
        const playermodelinfo &mdl = getplayermodelinfo(player1);
        for(int i = 0; i < Gun_NumGuns; ++i)
        {
            const char *file = guns[i].file;
            if(!file)
            {
                continue;
            }
            string fname;
            if(modecheck(gamemode, Mode_Team))
            {
                for(int j = 0; j < maxteams; ++j)
                {
                    formatstring(fname, "%s/%s", mdl.hudguns[1+j], file);
                    preloadmodel(fname);
                }
            }
            else
            {
                formatstring(fname, "%s/%s", mdl.hudguns[0], file);
                preloadmodel(fname);
            }
            formatstring(fname, "worldgun/%s", file);
            preloadmodel(fname);
        }
    }

    void preloadsounds()
    {
        for(int i = Sound_Jump; i <= Sound_Die2; i++)
        {
            preloadsound(i);
        }
    }

    void preloadworld()
    {
        preloadusedmapmodels(true); //extern from engine
        //internal game fxns (not from engine lib)
        if(hudgun)
        {
            preloadweapons();
        }
        preloadplayermodel();
        preloadsounds();
        //externs from engine below
        flushpreloadedmodels();
        preloadmapsounds();
        entitiesinoctanodes();
        attachentities();
        allchanged(true);
    }
    //============================================ edit cursor rendering ============================//

    void rendereditcursor()
    {
        int d   = DIMENSION(sel.orient),
            od  = DIMENSION(orient),
            odc = DIM_COORD(orient);
        bool hidecursor = UI::hascursor(),
             hovering   = false;
        hmapsel = false;

        if(moving)
        {
            static vec dest, handle;
            if(editmoveplane(vec(sel.o), camdir, od, sel.o[D[od]]+odc*sel.grid*sel.s[D[od]], handle, dest, moving==1))
            {
                if(moving==1)
                {
                    dest.add(handle);
                    handle = vec(ivec(handle).mask(~(sel.grid-1)));
                    dest.sub(handle);
                    moving = 2;
                }
                ivec o = ivec(dest).mask(~(sel.grid-1));
                sel.o[R[od]] = o[R[od]];
                sel.o[C[od]] = o[C[od]];
            }
        }
        else
        {
            ivec w;
            float sdist = 0,
                  wdist = 0,
                  t;
            int entorient = 0,
                ent = -1;

            wdist = rayent(player->o, camdir, 1e16f,
                           (editmode && showmat ? Ray_EditMat : 0)   // select cubes first
                           | (!dragging && entediting ? Ray_Ents : 0)
                           | Ray_SkipFirst
                           | (passthroughcube==1 ? Ray_Pass : 0), gridsize, entorient, ent);

            if((havesel || dragging) && !passthroughsel && !hmapedit)     // now try selecting the selection
                if(rayboxintersect(vec(sel.o), vec(sel.s).mul(sel.grid), player->o, camdir, sdist, orient))
                {   // and choose the nearest of the two
                    if(sdist < wdist)
                    {
                        wdist = sdist;
                        ent   = -1;
                    }
                }

            if((hovering = hoveringonent(hidecursor ? -1 : ent, entorient)))
            {
               if(!havesel)
               {
                   selchildcount = 0;
                   selchildmat = -1;
                   sel.s = ivec(0, 0, 0);
               }
            }
            else
            {
                vec w = vec(camdir).mul(wdist+0.05f).add(player->o);
                if(!insideworld(w))
                {
                    for(int i = 0; i < 3; ++i)
                    {
                        wdist = std::min(wdist, ((camdir[i] > 0 ? worldsize : 0) - player->o[i]) / camdir[i]);
                    }
                    w = vec(camdir).mul(wdist-0.05f).add(player->o);
                    if(!insideworld(w))
                    {
                        wdist = 0;
                        for(int i = 0; i < 3; ++i)
                        {
                            w[i] = std::clamp(player->o[i], 0.0f, float(worldsize));
                        }
                    }
                }
                cube *c = &lookupcube(ivec(w));
                if(gridlookup && !dragging && !moving && !havesel && hmapedit!=1)
                {
                    gridsize = lusize;
                }
                int mag = lusize / gridsize;
                normalizelookupcube(ivec(w));
                if(sdist == 0 || sdist > wdist)
                {
                    rayboxintersect(vec(lu), vec(gridsize), player->o, camdir, t=0, orient); // just getting orient
                }
                cur = lu;
                cor = ivec(vec(w).mul(2).div(gridsize));
                od = DIMENSION(orient);
                d = DIMENSION(sel.orient);

                if(hmapedit==1 && DIM_COORD(horient) == (camdir[DIMENSION(horient)]<0))
                {
                    hmapsel = hmap::isheightmap(horient, DIMENSION(horient), false, c);
                    if(hmapsel)
                    {
                        od = DIMENSION(orient = horient);
                    }
                }
                if(dragging)
                {
                    updateselection();
                    sel.cx   = std::min(cor[R[d]], lastcor[R[d]]);
                    sel.cy   = std::min(cor[C[d]], lastcor[C[d]]);
                    sel.cxs  = std::max(cor[R[d]], lastcor[R[d]]);
                    sel.cys  = std::max(cor[C[d]], lastcor[C[d]]);
                    if(!selectcorners)
                    {
                        sel.cx &= ~1;
                        sel.cy &= ~1;
                        sel.cxs &= ~1;
                        sel.cys &= ~1;
                        sel.cxs -= sel.cx-2;
                        sel.cys -= sel.cy-2;
                    }
                    else
                    {
                        sel.cxs -= sel.cx-1;
                        sel.cys -= sel.cy-1;
                    }
                    sel.cx  &= 1;
                    sel.cy  &= 1;
                    havesel = true;
                }
                else if(!havesel)
                {
                    sel.o = lu;
                    sel.s.x = sel.s.y = sel.s.z = 1;
                    sel.cx = sel.cy = 0;
                    sel.cxs = sel.cys = 2;
                    sel.grid = gridsize;
                    sel.orient = orient;
                    d = od;
                }
                sel.corner = (cor[R[d]]-(lu[R[d]]*2)/gridsize)+(cor[C[d]]-(lu[C[d]]*2)/gridsize)*2;
                selchildcount = 0;
                selchildmat = -1;
                countselchild(worldroot, ivec(0, 0, 0), worldsize/2);
                if(mag>=1 && selchildcount==1)
                {
                    selchildmat = c->material;
                    if(mag>1)
                    {
                        selchildcount = -mag;
                    }
                }
            }
        }

        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);

        // cursors

        ldrnotextureshader->set();

        renderentselection(player->o, camdir, entmoving!=0);

        boxoutline = outline!=0;

        enablepolygonoffset(GL_POLYGON_OFFSET_LINE);

        if(!moving && !hovering && !hidecursor)
        {
            if(hmapedit==1)
            {
                gle::colorub(0, hmapsel ? 255 : 40, 0);
            }
            else
            {
                gle::colorub(120,120,120);
            }
            boxs(orient, vec(lu), vec(lusize));
        }

        // selections
        if(havesel || moving)
        {
            d = DIMENSION(sel.orient);
            gle::colorub(50,50,50);   // grid
            boxsgrid(sel.orient, vec(sel.o), vec(sel.s), sel.grid);
            gle::colorub(200,0,0);    // 0 reference
            boxs3D(vec(sel.o).sub(0.5f*std::min(gridsize*0.25f, 2.0f)), vec(std::min(gridsize*0.25f, 2.0f)), 1);
            gle::colorub(200,200,200);// 2D selection box
            vec co(sel.o.v), cs(sel.s.v);
            co[R[d]] += 0.5f*(sel.cx*gridsize);
            co[C[d]] += 0.5f*(sel.cy*gridsize);
            cs[R[d]]  = 0.5f*(sel.cxs*gridsize);
            cs[C[d]]  = 0.5f*(sel.cys*gridsize);
            cs[D[d]] *= gridsize;
            boxs(sel.orient, co, cs);
            if(hmapedit==1)         // 3D selection box
            {
                gle::colorub(0,120,0);
            }
            else
            {
                gle::colorub(0,0,120);
            }
            boxs3D(vec(sel.o), vec(sel.s), sel.grid);
        }

        disablepolygonoffset(GL_POLYGON_OFFSET_LINE);

        boxoutline = false;

        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
    }
}
