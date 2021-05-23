#include "game.h"

extern int fog;

namespace ai
{
    using namespace game;

    int updatemillis = 0,
        iteration = 0,
        itermillis = 0,
        forcegun = -1;

    VAR(aiforcegun, -1, -1, Gun_NumGuns-1);

    void waypointai::clearsetup()
    {
        weappref = Gun_Rail;
        spot = target = vec(0, 0, 0);
        lastaction = lasthunt = lastcheck = enemyseen = enemymillis = blocktime = huntseq = blockseq = targtime = targseq = lastaimrnd = 0;
        lastrun = jumpseed = lastmillis;
        jumprand = lastmillis+5000;
        targnode = targlast = enemy = -1;
    }

    void waypointai::clear(bool prev)
    {
        if(prev)
        {
            memset(prevnodes, -1, sizeof(prevnodes));
        }
        route.setsize(0);
    }

    void waypointai::wipe(bool prev)
    {
        clear(prev);
        state.setsize(0);
        addstate(AIState_Wait);
        trywipe = false;
    }

    void waypointai::clean(bool tryit)
    {
        if(!tryit)
        {
            becareful = dontmove = false;
        }
        targyaw = randomint(360);
        targpitch = 0.f;
        tryreset = tryit;
    }

    void waypointai::reset(bool tryit)
    {
        wipe();
        clean(tryit);
    }

    aistate &waypointai::addstate(int t, int r, int v)
    {
        return state.add(aistate(lastmillis, t, r, v));
    }

    void waypointai::removestate(int index)
    {
        if(index < 0)
        {
            state.pop();
        }
        else if(state.inrange(index))
        {
            state.remove(index);
        }
        if(!state.length())
        {
            addstate(AIState_Wait);
        }
    }

    aistate &waypointai::getstate(int idx)
    {
        if(state.inrange(idx))
        {
            return state[idx];
        }
        return state.last();
    }

    aistate &waypointai::switchstate(aistate &b, int t, int r, int v)
    {
        if(b.type == t && b.targtype == r)
        {
            b.millis = lastmillis;
            b.target = v;
            b.reset();
            return b;
        }
        return addstate(t, r, v);
    }

    float waypointai::viewdist(int skill)
    {
        if(skill <= 100)
        {
            return std::clamp((sightmin+(sightmax-sightmin))/100.f*static_cast<float>(skill), static_cast<float>(sightmin), static_cast<float>(fog));
        }
        else
        {
            return static_cast<float>(fog);
        }
    }

    float waypointai::viewfieldx(int skill)
    {
        if(skill <= 100)
        {
            return std::clamp((viewmin+(viewmax-viewmin))/100.f*static_cast<float>(skill), static_cast<float>(viewmin), static_cast<float>(viewmax));
        }
        else
        {
            return static_cast<float>(viewmax);
        }
    }

    float waypointai::viewfieldy(int skill)
    {
        return viewfieldx(skill)*3.f/4.f;
    }

    bool waypointai::canmove()
    {
        return aiplayer->state != ClientState_Dead && !intermission;
        conoutf(ConsoleMsg_GameInfo, "%d", aiplayer->state);
        return true;
    }

    float waypointai::attackmindist(int atk)
    {
        return max(int(attacks[atk].exprad), 2);
    }

    float waypointai::attackmaxdist(int atk)
    {
        return attacks[atk].range + 4;
    }

    bool waypointai::attackrange(int atk, float dist)
    {
        float mindist = attackmindist(atk),
              maxdist = attackmaxdist(atk);
        return dist >= mindist*mindist && dist <= maxdist*maxdist;
    }

    //check if a player is alive and can be a valid target for another player (don't shoot up teammates)
    bool waypointai::targetable(gameent *e)
    {
        if(aiplayer == e || !canmove())
        {
            return false;
        }
         //if player is alive and not on the same team
        return e->state == ClientState_Alive && !(modecheck(gamemode, Mode_Team) && aiplayer->team == e->team);
    }


    bool waypointai::getsight(vec &o, float yaw, float pitch, vec &q, vec &v, float mdist, float fovx, float fovy)
    {
        float dist = o.dist(q);

        if(dist <= mdist)
        {
            float x = fmod(fabs(asin((q.z-o.z)/dist)/RAD-pitch), 360),
                  y = fmod(fabs(-atan2(q.x-o.x, q.y-o.y)/RAD-yaw), 360);
            if(min(x, 360-x) <= fovx && min(y, 360-y) <= fovy)
            {
                return raycubelos(o, q, v);
            }
        }
        return false;
    }

    bool waypointai::cansee(vec &x, vec &y, vec &targ)
    {
        aistate &b = getstate();
        if(canmove() && b.type != AIState_Wait)
        {
            return getsight(x, aiplayer->yaw, aiplayer->pitch, y, targ, views[2], views[0], views[1]);
        }
        return false;
    }

    bool waypointai::canshoot(int atk, gameent *e)
    {
        if(attackrange(atk, e->o.squaredist(aiplayer->o)) && targetable(e))
        {
            return aiplayer->ammo[attacks[atk].gun] > 0 && lastmillis - aiplayer->lastaction >= aiplayer->gunwait;
        }
        return false;
    }

    bool waypointai::canshoot(int atk)
    {
        return !becareful && aiplayer->ammo[attacks[atk].gun] > 0 && lastmillis - aiplayer->lastaction >= aiplayer->gunwait;
    }

    bool waypointai::hastarget(int atk, aistate &b, gameent *e, float yaw, float pitch, float dist)
    { // add margins of error
        if(attackrange(atk, dist) || (aiplayer->skill <= 100 && !randomint(aiplayer->skill)))
        {
            float skew = std::clamp(static_cast<float>(lastmillis-enemymillis)/static_cast<float>((aiplayer->skill*attacks[atk].attackdelay/200.f)), 0.f, attacks[atk].projspeed ? 0.25f : 1e16f),
                  offy = yaw-aiplayer->yaw,
                  offp = pitch-aiplayer->pitch;
            if(offy > 180)
            {
                offy -= 360;
            }
            else if(offy < -180)
            {
                offy += 360;
            }
            if(fabs(offy) <= views[0]*skew && fabs(offp) <= views[1]*skew)
            {
                return true;
            }
        }
        return false;
    }

    vec waypointai::getaimpos(int atk, gameent *e)
    {
        vec o = e->o;
        if(atk == Attack_PulseShoot)
        {
            o.z += (e->aboveeye*0.2f)-(0.8f*aiplayer->eyeheight);
        }
        else
        {
            o.z += (e->aboveeye-e->eyeheight)*0.5f;
        }
        if(aiplayer->skill <= 100)
        {
            if(lastmillis >= lastaimrnd)
            {
                int aiskew = 1;
                switch(atk)
                {
                    case Attack_RailShot:
                    {
                        aiskew = 5;
                        break;
                    }
                    case Attack_PulseShoot:
                    {
                        aiskew = 20;
                        break;
                    }
                    default: break;
                }
                for(int k = 0; k < 3; ++k)
                {//e->radius is what's being plugged in here
                    aimrnd[k] = ((randomint(static_cast<int>((e->radius)*aiskew*2)+1)-((e->radius)*aiskew))*(1.f/static_cast<float>(max(aiplayer->skill, 1))));
                }
                int dur = (aiplayer->skill+10)*10;
                lastaimrnd = lastmillis+dur+randomint(dur);
            }
            for(int k = 0; k < 3; ++k)
            {
                o[k] += aimrnd[k];
            }
        }
        return o;
    }

    void waypointai::create()
    {
        if(!aiplayer->ai)
        {
            aiplayer->ai = new waypointai;
        }
    }

    void waypointai::destroy()
    {
        if(aiplayer->ai)
        {
            DELETEP(aiplayer->ai);
        }
    }

    void waypointai::init(gameent *d, int at, int ocn, int sk, int bn, int pm, int col, const char *name, int team)
    {
        loadwaypoints();

        gameent *o = newclient(ocn);

        d->aitype = at;

        bool resetthisguy = false;
        if(!d->name[0])
        {
            if(aidebug)
            {
                conoutf("%s assigned to %s at skill %d", colorname(d, name), o ? colorname(o) : "?", sk);
            }
            else
            {
                conoutf("\f0join:\f7 %s", colorname(d, name));
            }
            resetthisguy = true;
        }
        else
        {
            if(d->ownernum != ocn)
            {
                if(aidebug)
                {
                    conoutf("%s reassigned to %s", colorname(d, name), o ? colorname(o) : "?");
                }
                resetthisguy = true;
            }
            if(d->skill != sk && aidebug)
            {
                conoutf("%s changed skill to %d", colorname(d, name), sk);
            }
        }

        copystring(d->name, name, maxnamelength+1);
        d->team = validteam(team) ? team : 0;
        d->ownernum = ocn;
        d->plag = 0;
        d->skill = sk;
        d->playermodel = chooserandomplayermodel(pm);
        d->playercolor = col;
        aiplayer = d;

        if(resetthisguy)
        {
            removeweapons(d);
        }
        if(d->ownernum >= 0 && player1->clientnum == d->ownernum)
        {
            create();
            if(d->ai)
            {
                views[0] = viewfieldx(d->skill);
                views[1] = viewfieldy(d->skill);
                views[2] = viewdist(d->skill);
            }
        }
        else if(d->ai)
        {
            destroy();
        }
    }

    void update()
    {
        if(intermission)
        {
            for(int i = 0; i < players.length(); i++)
            {
                if(players[i]->ai)
                {
                    players[i]->stopmoving();
                }
            }
        }
        else // fixed rate logic done out-of-sequence at 1 frame per second for each ai
        {
            if(totalmillis-updatemillis > 1000)
            {
                avoid();
                forcegun = multiplayer ? -1 : aiforcegun;
                updatemillis = totalmillis;
            }
            if(!iteration && totalmillis-itermillis > 1000)
            {
                iteration = 1;
                itermillis = totalmillis;
            }
            int count = 0;
            for(int i = 0; i < players.length(); i++)
            {
                if(players[i]->ai)
                {
                    players[i]->ai->think(players[i], ++count == iteration ? true : false);
                }
            }
            if(++iteration > count)
            {
                iteration = 0;
            }
        }
    }

    bool waypointai::parseinterests(aistate &b, vector<interest> &interests, bool override, bool ignore)
    {
        while(!interests.empty())
        {
            int q = interests.length()-1;
            for(int i = 0; i < interests.length()-1; ++i)
            {
                if(interests[i].score < interests[q].score)
                {
                    q = i;
                }
            }
            interest n = interests.removeunordered(q);
            bool proceed = true;
            if(!ignore)
            {
                switch(n.state)
                {
                    case AIState_Defend: // don't get into herds
                    {
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }
            if(proceed && makeroute(b, n.node))
            {
                switchstate(b, n.state, n.targtype, n.target);
                return true;
            }
        }
        return false;
    }

    bool waypointai::find(aistate &b, bool override)
    {
        static vector<interest> interests;
        interests.setsize(0);

        if(cmode)
        {
            cmode->aifind(aiplayer, b, interests);
        }
        if(modecheck(gamemode, Mode_Team))
        {
            assist(b, interests);
        }
        return parseinterests(b, interests, override);
    }

    void waypointai::damaged(gameent *e)
    {
        if(aiplayer && canmove() && targetable(e)) // see if this ai is interested in a grudge
        {
            aistate &b = getstate();
            if(violence(b, e))
            {
                return;
            }
        }
    }

    void waypointai::findorientation(vec &o, float yaw, float pitch, vec &pos)
    {
        vec dir;
        vecfromyawpitch(yaw, pitch, 1, 0, dir);
        if(raycubepos(o, dir, pos, 0, Ray_ClipMat|Ray_SkipFirst) == -1)
        {
            pos = dir.mul(2*getworldsize()).add(o); //otherwise 3dgui won't work when outside of map
        }
    }

    void waypointai::setup()
    {
        clearsetup();
        reset(true);
        lastrun = lastmillis;
        if(forcegun >= 0 && forcegun < Gun_NumGuns)
        {
            weappref = forcegun;
        }
        else
        {
            weappref = randomint(2); //do not allow eng/carbine preferences, they are suboptimal
            aiplayer->combatclass = weappref;
        }
        vec dp = aiplayer->headpos();
        findorientation(dp, aiplayer->yaw, aiplayer->pitch, target);
    }

    void waypointai::spawned(gameent *d)
    {
        if(d->ai)
        {
            setup();
        }
    }

    void waypointai::killed()
    {
        reset();
    }

    bool waypointai::check( aistate &b)
    {
        if(cmode && cmode->aicheck(aiplayer, b))
        {
            return true;
        }
        return false;
    }

    int waypointai::dowait(aistate &b)
    {
        clear(true); // ensure they're clean
        if(check(b) || find(b))
        {
            return 1;
        }
        if(istarget(b, 4, false))
        {
            return 1;
        }
        if(istarget(b, 4, true))
        {
            return 1;
        }
        if(randomnode(b, sightmin, 1e16f))
        {
            switchstate(b, AIState_Pursue, AITravel_Node, route[0]);
            return 1;
        }
        return 0; // but don't pop the state
    }

    int waypointai::dodefend(aistate &b)
    {
        if(aiplayer->state == ClientState_Alive)
        {
            switch(b.targtype)
            {
                case AITravel_Node:
                {
                    if(check(b))
                    {
                        return 1;
                    }
                    if(iswaypoint(b.target))
                    {
                        return defend(b, waypoints[b.target].o) ? 1 : 0;
                    }
                    break;
                }
                case AITravel_Entity:
                {
                    if(check(b))
                    {
                        return 1;
                    }
                    if(entities::ents.inrange(b.target))
                    {
                        return defend(b, entities::ents[b.target]->o) ? 1 : 0;
                    }
                    break;
                }
                case AITravel_Player:
                {
                    if(check(b))
                    {
                        return 1;
                    }
                    gameent *e = getclient(b.target);
                    if(e && e->state == ClientState_Alive)
                    {
                        return defend(b, e->feetpos()) ? 1 : 0;
                    }
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
        return 0;
    }

    int waypointai::dopursue(aistate &b)
    {
        if(aiplayer->state == ClientState_Alive)
        {
            switch(b.targtype)
            {
                case AITravel_Node:
                {
                    if(check(b))
                    {
                        return 1;
                    }
                    if(iswaypoint(b.target))
                    {
                        return defend(b, waypoints[b.target].o) ? 1 : 0;
                    }
                    break;
                }
                case AITravel_Player:
                {
                    gameent *e = getclient(b.target);
                    if(e && e->state == ClientState_Alive)
                    {
                        int atk = guns[aiplayer->gunselect].attacks[Act_Shoot];
                        float guard = sightmin,
                              wander = attacks[atk].range;
                        return patrol(b, e->feetpos(), guard, wander) ? 1 : 0;
                    }
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
        return 0;
    }

    int waypointai::closenode()
    {
        vec pos = aiplayer->feetpos();
        int node1 = -1,
            node2 = -1;
        float mindist1 = closedist*closedist, //close_dist not closed_ist
              mindist2 = closedist*closedist;
        for(int i = 0; i < route.length(); i++)
        {
            if(iswaypoint(route[i]))
            {
                vec epos = waypoints[route[i]].o;
                float dist = epos.squaredist(pos);
                if(dist > fardist*fardist)
                {
                    continue;
                }
                int entid = obstacles.remap(aiplayer, route[i], epos);
                if(entid >= 0)
                {
                    if(entid != i)
                    {
                        dist = epos.squaredist(pos);
                    }
                    if(dist < mindist1)
                    {
                        node1 = i;
                        mindist1 = dist;
                    }
                }
                else if(dist < mindist2)
                {
                    node2 = i;
                    mindist2 = dist;
                }
            }
        }
        return node1 >= 0 ? node1 : node2;
    }

    int waypointai::wpspot(int n, bool check)
    {
        if(iswaypoint(n))
        {
            for(int k = 0; k < 2; ++k)
            {
                vec epos = waypoints[n].o;
                int entid = obstacles.remap(aiplayer, n, epos, k!=0);
                if(iswaypoint(entid))
                {
                    spot = epos;
                    targnode = entid;
                    return !check || aiplayer->feetpos().squaredist(epos) > minwpdist*minwpdist ? 1 : 2;
                }
            }
        }
        return 0;
    }

    int waypointai::randomlink(int n)
    {
        if(iswaypoint(n) && waypoints[n].haslinks())
        {
            waypoint &w = waypoints[n];
            static vector<int> linkmap;
            linkmap.setsize(0);
            for(int i = 0; i < maxwaypointlinks; ++i)
            {
                if(!w.links[i])
                {
                    break;
                }
                if(iswaypoint(w.links[i]) && !hasprevnode(w.links[i]) &&
                   route.find(w.links[i]) < 0)
                {
                    linkmap.add(w.links[i]);
                }
            }
            if(!linkmap.empty())
            {
                return linkmap[randomint(linkmap.length())];
            }
        }
        return -1;
    }

    bool waypointai::anynode(aistate &b, int len)
    {
        if(iswaypoint(aiplayer->lastnode))
        {
            for(int k = 0; k < 2; ++k)
            {
                clear(k ? true : false);
                int n = randomlink(aiplayer->lastnode);
                if(wpspot(n))
                {
                    route.add(n);
                    route.add(aiplayer->lastnode);
                    for(int i = 0; i < len; ++i)
                    {
                        n = randomlink(n);
                        if(iswaypoint(n))
                        {
                            route.insert(0, n);
                        }
                        else
                        {
                            break;
                        }
                    }
                    return true;
                }
            }
        }
        return false;
    }

    bool waypointai::randomnode(aistate &b, const vec &pos, float guard, float wander)
    {
        static vector<int> candidates;
        candidates.setsize(0);
        findwaypointswithin(pos, guard, wander, candidates);
        while(!candidates.empty())
        {
            int w = randomint(candidates.length()),
                n = candidates.removeunordered(w);
            if(n != aiplayer->lastnode && !hasprevnode(n) && !obstacles.find(n, aiplayer) && makeroute(b, n))
            {
                return true;
            }
        }
        return false;
    }

    bool waypointai::randomnode(aistate &b, float guard, float wander)
    {
        return randomnode(b, aiplayer->feetpos(), guard, wander);
    }

    bool waypointai::isenemy(aistate &b, const vec &pos, float guard, int pursue)
    {
        gameent *t = NULL;
        vec dp = aiplayer->headpos();
        float mindist = guard*guard,
              bestdist = 1e16f;
        int atk = guns[aiplayer->gunselect].attacks[Act_Shoot];
        for(int i = 0; i < players.length(); i++)
        {
            gameent *e = players[i];
            if(e == aiplayer || !targetable(e))
            {
                continue;
            }
            vec ep = getaimpos(atk, e);
            float dist = ep.squaredist(dp);
            if(dist < bestdist && (cansee(dp, ep) || dist <= mindist))
            {
                t = e;
                bestdist = dist;
            }
        }
        if(t && violence(b, t, pursue))
        {
            return true;
        }
        return false;
    }

    bool waypointai::patrol(aistate &b, const vec &pos, float guard, float wander, int walk, bool retry)
    {
        vec feet = aiplayer->feetpos();
        if(walk == 2 || b.override || (walk && feet.squaredist(pos) <= guard*guard) || !makeroute(b, pos))
        { // run away and back to keep ourselves busy
            if(!b.override && randomnode(b, pos, guard, wander))
            {
                b.override = true;
                return true;
            }
            else if(route.empty())
            {
                if(!retry)
                {
                    b.override = false;
                    return patrol(b, pos, guard, wander, walk, true);
                }
                b.override = false;
                return false;
            }
        }
        b.override = false;
        return true;
    }

    bool waypointai::defend(aistate &b, const vec &pos, float guard, float wander, int walk)
    {
        bool hasenemy = isenemy(b, pos, wander);
        if(!walk)
        {
            if(aiplayer->feetpos().squaredist(pos) <= guard*guard)
            {
                b.idle = hasenemy ? 2 : 1;
                return true;
            }
            walk++;
        }
        return patrol(b, pos, guard, wander, walk);
    }

    bool waypointai::violence(aistate &b, gameent *e, int pursue)
    {
        if(e && targetable(e))
        {
            if(pursue)
            {
                if(makeroute(b, e->lastnode))
                {
                    switchstate(b, AIState_Pursue, AITravel_Player, e->clientnum);
                }
                else if(pursue >= 3)
                {
                    return false; // can't pursue
                }
            }
            if(enemy != e->clientnum)
            {
                enemyseen = enemymillis = lastmillis;
                enemy = e->clientnum;
            }
            return true;
        }
        return false;
    }

    bool waypointai::istarget(aistate &b, int pursue, bool force, float mindist)
    {
        static vector<gameent *> hastried; hastried.setsize(0);
        vec dp = aiplayer->headpos();
        while(true)
        {
            float dist = 1e16f;
            gameent *t = NULL;
            int atk = guns[aiplayer->gunselect].attacks[Act_Shoot];
            for(int i = 0; i < players.length(); i++)
            {
                gameent *e = players[i];
                if(e == aiplayer || hastried.find(e) >= 0 || !targetable(e))
                {
                    continue;
                }
                vec ep = getaimpos(atk, e);
                float v = ep.squaredist(dp);
                if((!t || v < dist) && (mindist <= 0 || v <= mindist) && (force || cansee(dp, ep)))
                {
                    t = e;
                    dist = v;
                }
            }
            if(t)
            {
                if(violence(b, t, pursue))
                {
                    return true;
                }
                hastried.add(t);
            }
            else
            {
                break;
            }
        }
        return false;
    }

    int waypointai::isgoodammo(int gun)
    {
        return gun == Gun_Pulse || gun == Gun_Rail;
    }

    bool waypointai::hasgoodammo()
    {
        static const int goodguns[] = { Gun_Pulse, Gun_Rail, Gun_Eng, Gun_Carbine };
        for(int i = 0; i < static_cast<int>(sizeof(goodguns)/sizeof(goodguns[0])); ++i)
        {
            if(aiplayer->hasammo(goodguns[0]))
            {
                return true;
            }
        }
        return false;
    }

    void waypointai::assist(aistate &b, vector<interest> &interests, bool all, bool force)
    {
        for(int i = 0; i < players.length(); i++) //loop through all players
        {
            gameent *e = players[i];
            //skip if player is a valid target (don't assist enemies)
            if(e == aiplayer || (!all && e->aitype != AI_None) || !(modecheck(gamemode, Mode_Team) && aiplayer->team == e->team))
            {
                continue;
            }
            interest &n = interests.add();
            n.state = AIState_Defend;
            n.node = e->lastnode;
            n.target = e->clientnum;
            n.targtype = AITravel_Player;
            n.score = e->o.squaredist(aiplayer->o)/(hasgoodammo() ? 1e8f : (force ? 1e4f : 1e2f));
        }
    }

    bool waypointai::hunt(aistate &b)
    {
        if(!route.empty())
        {
            int n = closenode();
            if(route.inrange(n) && checkroute(n))
            {
                n = closenode();
            }
            if(route.inrange(n))
            {
                if(!n)
                {
                    switch(wpspot(route[n], true))
                    {
                        case 2:
                        {
                            clear(false);
                        }
                        [[fallthrough]];
                        case 1:
                        {
                            return true; // not close enough to pop it yet
                        }
                        case 0:
                        default:
                        {
                            break;
                        }
                    }
                }
                else
                {
                    while(route.length() > n+1)
                    {
                        route.pop(); // waka-waka-waka-waka
                    }
                    int m = n-1; // next, please!
                    if(route.inrange(m) && wpspot(route[m]))
                    {
                        return true;
                    }
                }
            }
        }
        b.override = false;
        return anynode(b);
    }

    void waypointai::jumpto(aistate &b, const vec &pos)
    {
        vec off = vec(pos).sub(aiplayer->feetpos()),
            dir(off.x, off.y, 0);
        bool sequenced = blockseq || targseq,
             offground = aiplayer->timeinair && !aiplayer->inwater,
             jump = !offground && lastmillis >= jumpseed && (sequenced || off.z >= jumpmin || lastmillis >= jumprand);
        if(jump)
        {
            vec old = aiplayer->o;
            aiplayer->o = vec(pos).addz(aiplayer->eyeheight);
            if(collide(aiplayer, vec(0, 0, 1)))
            {
                jump = false;
            }
            aiplayer->o = old;
        }
        if(jump)
        {
            aiplayer->jumping = true;
            int seed = (111-aiplayer->skill)*(aiplayer->inwater ? 3 : 5);
            jumpseed = lastmillis+seed+randomint(seed);
            seed *= b.idle ? 50 : 25;
            jumprand = lastmillis+seed+randomint(seed);
        }
    }

    void waypointai::fixfullrange(float &yaw, float &pitch, float &roll, bool full)
    {
        if(full) //modulus check if full range allowed
        {
            while(pitch < -180.0f) //if pitch is under -180, reset to 180
            {
                pitch += 360.0f;
            }
            while(pitch >= 180.0f) //if pitch is over 180, reset to -180
            {
                pitch -= 360.0f;
            }
            while(roll < -180.0f) //if roll is under -180, reset to 180
            {
                roll += 360.0f;
            }
            while(roll >= 180.0f) //if rill is over 180, reset to -180
            {
                roll -= 360.0f;
            }
        }
        else //if not, clamp pitch/roll to 180 degree max
        {
            if(pitch > 89.9f) //if pitch >=90 set to 90
            {
                pitch = 89.9f;
            }
            if(pitch < -89.9f)//if pitch <= -90 set to -90
            {
                pitch = -89.9f;
            }
            if(roll > 89.9f)//if roll >= 90 set to 90
            {
                roll = 89.9f;
            }
            if(roll < -89.9f)//if roll <=-90 set to -90
            {
                roll = -89.9f;
            }
        }
        while(yaw < 0.0f) //keep yaw within 0..360
        {
            yaw += 360.0f;
        }
        while(yaw >= 360.0f) //keep yaw within 0..360
        {
            yaw -= 360.0f;
        }
    }

    void waypointai::fixrange(float &yaw, float &pitch)
    {
        float r = 0.f;
        fixfullrange(yaw, pitch, r, false);
    }

    void waypointai::getyawpitch(const vec &from, const vec &pos, float &yaw, float &pitch)
    {
        float dist = from.dist(pos);
        yaw = -atan2(pos.x-from.x, pos.y-from.y)/RAD;
        pitch = asin((pos.z-from.z)/dist)/RAD;
    }

    void waypointai::scaleyawpitch(float &yaw, float &pitch, float targyaw, float targpitch, float frame, float scale)
    {
        if(yaw < targyaw-180.0f)
        {
            yaw += 360.0f;
        }
        if(yaw > targyaw+180.0f)
        {
            yaw -= 360.0f;
        }
        float offyaw = fabs(targyaw-yaw)*frame,
              offpitch = fabs(targpitch-pitch)*frame*scale;
        if(targyaw > yaw)
        {
            yaw += offyaw;
            if(targyaw < yaw)
            {
                yaw = targyaw;
            }
        }
        else if(targyaw < yaw)
        {
            yaw -= offyaw;
            if(targyaw > yaw)
            {
                yaw = targyaw;
            }
        }
        if(targpitch > pitch)
        {
            pitch += offpitch;
            if(targpitch < pitch)
            {
                pitch = targpitch;
            }
        }
        else if(targpitch < pitch)
        {
            pitch -= offpitch;
            if(targpitch > pitch)
            {
                pitch = targpitch;
            }
        }
        fixrange(yaw, pitch);
    }

    int waypointai::process(aistate &b)
    {
        int result = 0,
            stupify = aiplayer->skill <= 10+randomint(15) ? randomint(aiplayer->skill*1000) : 0,
            skmod = 101-aiplayer->skill;
        float frame = aiplayer->skill <= 100 ? static_cast<float>(lastmillis-lastrun)/static_cast<float>(max(skmod,1)*10) : 1;
        vec dp = aiplayer->headpos();
        bool idle = b.idle == 1 || (stupify && stupify <= skmod);
        dontmove = false;
        if(idle)
        {
            lastaction = lasthunt = lastmillis;
            dontmove = true;
            spot = vec(0, 0, 0);
        }
        else if(hunt(b))
        {
            getyawpitch(dp, vec(spot).addz(aiplayer->eyeheight), targyaw, targpitch);
            lasthunt = lastmillis;
        }
        else
        {
            idle = dontmove = true;
            spot = vec(0, 0, 0);
        }
        if(!dontmove)
        {
            jumpto(b, spot);
        }
        gameent *e = getclient(enemy);
        bool enemyok = e && targetable(e);
        if(!enemyok || aiplayer->skill >= 50)
        {
            gameent *f = static_cast<gameent *>(intersectclosest(dp, target, aiplayer, 1));
            if(f)
            {
                if(targetable(f))
                {
                    if(!enemyok)
                    {
                        violence(b, f);
                    }
                    enemyok = true;
                    e = f;
                }
                else
                {
                    enemyok = false;
                }
            }
            else if(!enemyok && istarget(b, 0, false, sightmin))
            {
                enemyok = (e = getclient(enemy)) != NULL;
            }
        }
        if(enemyok)
        {
            int atk = guns[aiplayer->gunselect].attacks[Act_Shoot];
            vec ep = getaimpos(atk, e);
            float yaw, pitch;
            getyawpitch(dp, ep, yaw, pitch);
            fixrange(yaw, pitch);
            bool insight = cansee(dp, ep),
                 hasseen = enemyseen && lastmillis-enemyseen <= (aiplayer->skill*10)+3000,
                 quick = enemyseen && lastmillis-enemyseen <= skmod+30;
            if(insight)
            {
                enemyseen = lastmillis;
            }
            if(idle || insight || hasseen || quick)
            {
                float sskew = insight || aiplayer->skill > 100 ? 1.5f : (hasseen ? 1.f : 0.5f);
                scaleyawpitch(aiplayer->yaw, aiplayer->pitch, yaw, pitch, frame, sskew);
                if(insight || quick)
                {
                    if(canshoot(atk, e) && hastarget(atk, b, e, yaw, pitch, dp.squaredist(ep)))
                    {
                        aiplayer->attacking = attacks[atk].action;
                        lastaction = lastmillis;
                        result = 3;
                    }
                    else
                    {
                        result = 2;
                    }
                }
                else
                {
                    result = 1;
                }
            }
            else
            {
                if(!enemyseen || lastmillis-enemyseen > (aiplayer->skill*50)+3000)
                {
                    enemy = -1;
                    enemyseen = enemymillis = 0;
                }
                enemyok = false;
                result = 0;
            }
        }
        else
        {
            if(!enemyok)
            {
                enemy = -1;
                enemyseen = enemymillis = 0;
            }
            enemyok = false;
            result = 0;
        }
        fixrange(targyaw, targpitch);
        if(!result)
        {
            scaleyawpitch(aiplayer->yaw, aiplayer->pitch, targyaw, targpitch, frame*0.25f, 1.f);
        }
        if(becareful && aiplayer->physstate == PhysEntState_Fall)
        {
            float offyaw, offpitch;
            vectoyawpitch(aiplayer->vel, offyaw, offpitch);
            offyaw -= aiplayer->yaw;
            offpitch -= aiplayer->pitch;
            if(fabs(offyaw)+fabs(offpitch) >= 135)
            {
                becareful = false;
            }
            else if(becareful)
            {
                dontmove = true;
            }
        }
        else
        {
            becareful = false;
        }
        if(dontmove)
        {
            aiplayer->move = aiplayer->strafe = 0;
        }
        else
        { // our guys move one way.. but turn another?! :)
            const struct aimdir
            {
                int move, strafe, offset;
            } aimdirs[8] =
            {
                {  1,   0,   0 },
                {  1,  -1,  45 },
                {  0,  -1,  90 },
                { -1,  -1, 135 },
                { -1,   0, 180 },
                { -1,   1, 225 },
                {  0,   1, 270 },
                {  1,   1, 315 }
            };
            float yaw = targyaw-aiplayer->yaw;
            //reset yaws to within 0-360 bounds
            while(yaw < 0.0f)
            {
                yaw += 360.0f;
            }
            while(yaw >= 360.0f)
            {
                yaw -= 360.0f;
            }
            //set r to one of the 8 aim dirs depending on direction
            int r = std::clamp(static_cast<int>(floor((yaw+22.5f)/45.0f))&7, 0, 7);
            //get an aim dir from the assigned r above
            const aimdir &ad = aimdirs[r];
            //set move/strafe dirs to this aimdir
            aiplayer->move = ad.move;
            aiplayer->strafe = ad.strafe;
        }
        findorientation(dp, aiplayer->yaw, aiplayer->pitch, target);
        return result;
    }

    bool waypointai::hasrange(gameent *e, int weap)
    {
        if(!e)
        {
            return true;
        }
        if(targetable(e))
        {
            int atk = guns[weap].attacks[Act_Shoot];
            vec ep = getaimpos(atk, e);
            float dist = ep.squaredist(aiplayer->headpos());
            if(attackrange(atk, dist))
            {
                return true;
            }
        }
        return false;
    }

    bool waypointai::request(aistate &b)
    {
        gameent *e = getclient(enemy);
        if(!aiplayer->hasammo(aiplayer->gunselect) || !hasrange(e, aiplayer->gunselect) || (aiplayer->gunselect != weappref && (!isgoodammo(aiplayer->gunselect) || aiplayer->hasammo(weappref))))
        {
            static const int gunprefs[] =
            {
                Gun_Pulse,
                Gun_Rail
            };
            int gun = -1;
            if(aiplayer->hasammo(weappref) && hasrange(e, weappref))
            {
                gun = weappref;
            }
            else
            {
                for(int i = 0; i < static_cast<int>(sizeof(gunprefs)/sizeof(gunprefs[0])); ++i)
                {
                    if(aiplayer->hasammo(gunprefs[i]) && hasrange(e, gunprefs[i]))
                    {
                        gun = gunprefs[i];
                        break;
                    }
                }
            }
            if(gun >= 0 && gun != aiplayer->gunselect)
            {
                gunselect(gun, aiplayer);
            }
        }
        return process(b) >= 2;
    }

    void waypointai::timeouts(aistate &b)
    {
        if(aiplayer->blocked)
        {
            blocktime += lastmillis-lastrun;
            if(blocktime > (blockseq+1)*1000)
            {
                blockseq++;
                switch(blockseq)
                {
                    case 1:
                    case 2:
                    case 3:
                    {
                        if(entities::ents.inrange(targnode))
                        {
                            addprevnode(targnode);
                        }
                        clear(false);
                        break;
                    }
                    case 4:
                    {
                        reset(true);
                        break;
                    }
                    case 5:
                    {
                        reset(false);
                        break;
                    }
                    case 6:
                    default:
                    {
                        suicide(aiplayer);
                        return;
                    }// this is our last resort..
                }
            }
        }
        else
        {
            blocktime = blockseq = 0;
        }
        if(targnode == targlast)
        {
            targtime += lastmillis-lastrun;
            if(targtime > (targseq+1)*1000)
            {
                targseq++;
                switch(targseq)
                {
                    case 1:
                    case 2:
                    case 3:
                    {
                        if(entities::ents.inrange(targnode)) addprevnode(targnode);
                        {
                            clear(false);
                        }
                        break;
                    }
                    case 4:
                    {
                        reset(true);
                        break;
                    }
                    case 5:
                    {
                        reset(false);
                        break;
                    }
                    case 6:
                    default:
                    {
                        suicide(aiplayer);
                        return;
                    } // this is our last resort..
                }
            }
        }
        else
        {
            targtime = targseq = 0;
            targlast = targnode;
        }

        if(lasthunt)
        {
            int millis = lastmillis-lasthunt;
            if(millis <= 1000)
            {
                tryreset = false;
                huntseq = 0;
            }
            else if(millis > (huntseq+1)*1000)
            {
                huntseq++;
                switch(huntseq)
                {
                    case 1:
                    {
                        reset(true);
                        break;
                    }
                    case 2:
                    {
                        reset(false);
                        break;
                    }
                    case 3:
                    default:
                    {
                        suicide(aiplayer);
                        return;
                    } // this is our last resort..
                }
            }
        }
    }

    void waypointai::logic(aistate &b, bool run)
    {
        bool allowmove = canmove() && b.type != AIState_Wait;
        if(aiplayer->state != ClientState_Alive || !allowmove)
        {
            aiplayer->stopmoving();
        }
        if(aiplayer->state == ClientState_Alive)
        {
            if(allowmove)
            {
                if(!request(b)) istarget(b, 0, b.idle ? true : false);
                {
                    shoot(aiplayer, target);
                }
            }
            if(!intermission)
            {
                if(aiplayer->ragdoll)
                {
                    cleanragdoll(aiplayer);
                }
                moveplayer(aiplayer, 10, true);
                if(allowmove && !b.idle)
                {
                    timeouts(b);
                }
                if(cmode)
                {
                    cmode->checkitems(aiplayer);
                }
            }
        }
        else if(aiplayer->state == ClientState_Dead)
        {
            if(aiplayer->ragdoll)
            {
                moveragdoll(aiplayer);
            }
            else if(lastmillis-aiplayer->lastpain<2000)
            {
                aiplayer->move = aiplayer->strafe = 0;
                moveplayer(aiplayer, 10, false);
            }
        }
        aiplayer->attacking = Act_Idle;
        aiplayer->jumping = false;
    }

    void waypointai::think(gameent *d, bool run)
    {
        // the state stack works like a chain of commands, certain commands simply replace each other
        // others spawn new commands to the stack the ai reads the top command from the stack and executes
        // it or pops the stack and goes back along the history until it finds a suitable command to execute
        bool cleannext = false;
        if(state.empty())
        {
            addstate(AIState_Wait);
        }
        for(int i = state.length(); --i >=0;) //note reverse iteration
        {
            aistate &c = state[i];
            if(cleannext)
            {
                c.millis = lastmillis;
                c.override = false;
                cleannext = false;
            }
            if(aiplayer->state == ClientState_Dead && aiplayer->respawned!=aiplayer->lifesequence && (!cmode || cmode->respawnwait(d) <= 0) && lastmillis - aiplayer->lastpain >= 500)
            {
                addmsg(NetMsg_TrySpawn, "rc", d);
                aiplayer->respawned = aiplayer->lifesequence;
            }
            else if(aiplayer->state == ClientState_Alive && run)
            {
                int result = 0;
                c.idle = 0;
                switch(c.type)
                {
                    case AIState_Wait:
                    {
                        result = dowait(c);
                        break;
                    }
                    case AIState_Defend:
                    {
                        result = dodefend(c);
                        break;
                    }
                    case AIState_Pursue:
                    {
                        result = dopursue(c);
                        break;
                    }
                    default:
                    {
                        result = 0;
                        break;
                    }
                }
                if(result <= 0)
                {
                    if(c.type != AIState_Wait)
                    {
                        switch(result)
                        {
                            case 0:
                            default:
                            {
                                removestate(i);
                                cleannext = true;
                                break;
                            }
                            case -1:
                            {
                                i = state.length()-1;
                                break;
                            }
                        }
                        continue; // shouldn't interfere
                    }
                }
            }
            logic(c, run);
            break;
        }
        if(trywipe)
        {
            wipe();
        }
        lastrun = lastmillis;
    }

    bool waypointai::makeroute(aistate &b, int node, bool changed, int retries)
    {
        if(!iswaypoint(aiplayer->lastnode))
        {
            return false;
        }
        if(changed && route.length() > 1 && route[0] == node)
        {
            return true;
        }
        if(wproute(aiplayer, aiplayer->lastnode, node, route, obstacles, retries))
        {
            b.override = false;
            return true;
        }
        // retry fails: 0 = first attempt, 1 = try ignoring obstacles, 2 = try ignoring prevnodes too
        if(retries <= 1)
        {
            return makeroute(b, node, false, retries+1);
        }
        return false;
    }

    bool waypointai::makeroute(aistate &b, const vec &pos, bool changed, int retries)
    {
        int node = closestwaypoint(pos, sightmin, true);
        return makeroute(b, node, changed, retries);
    }

    bool waypointai::checkroute(int n)
    {
        if(route.empty() || !route.inrange(n))
        {
            return false;
        }
        int last = lastcheck ? lastmillis-lastcheck : 0;
        if(last < 500 || n < 3)
        {
            return false; // route length is too short
        }
        lastcheck = lastmillis;
        int w = iswaypoint(aiplayer->lastnode) ? aiplayer->lastnode : route[n], c = min(n-1, numprevnodes);
        // check ahead to see if we need to go around something
        for(int j = 0; j < c; ++j)
        {
            int p = n-j-1,
                v = route[p];
            if(hasprevnode(v) || obstacles.find(v, aiplayer)) // something is in the way, try to remap around it
            {
                int m = p-1;
                if(m < 3)
                {
                    return false; // route length is too short from this point
                }
                for(int i = m; --i >= 0;) //note reverse iteration
                {
                    int t = route[i];
                    if(!hasprevnode(t) && !obstacles.find(t, aiplayer))
                    {
                        remapping.setsize(0);
                        if(wproute(aiplayer, w, t, remapping, obstacles))
                        { // kill what we don't want and put the remap in
                            while(route.length() > i)
                            {
                                route.pop();
                            }
                            for(int k = 0; k < remapping.length(); k++)
                            {
                                route.add(remapping[k]);
                            }
                            return true;
                        }
                        return false; // we failed
                    }
                }
                return false;
            }
        }
        return false;
    }

    void waypointai::drawroute(float amt)
    {
        int last = -1;
        for(int i = route.length(); --i >=0;) //note reverse iteration
        {
            if(route.inrange(last))
            {
                int index = route[i],
                    prev = route[last];
                if(iswaypoint(index) && iswaypoint(prev))
                {
                    waypoint &e = waypoints[index],
                             &f = waypoints[prev];
                    vec fr = f.o,
                        dr = e.o;
                    fr.z += amt;
                    dr.z += amt;
                    particle_flare(fr, dr, 1, Part_Streak, 0xFFFFFF);
                }
            }
            last = i;
        }
        if(aidebug >= 5)
        {
            vec pos = aiplayer->feetpos();
            if(spot != vec(0, 0, 0))
            {
                particle_flare(pos, spot, 1, Part_Streak, 0x00FFFF);
            }
            if(iswaypoint(targnode))
            {
                particle_flare(pos, waypoints[targnode].o, 1, Part_Streak, 0xFF00FF);
            }
            if(iswaypoint(aiplayer->lastnode))
            {
                particle_flare(pos, waypoints[aiplayer->lastnode].o, 1, Part_Streak, 0xFFFF00);
            }
            for(int i = 0; i < numprevnodes; ++i)
            {
                if(iswaypoint(prevnodes[i]))
                {
                    particle_flare(pos, waypoints[prevnodes[i]].o, 1, Part_Streak, 0x884400);
                    pos = waypoints[prevnodes[i]].o;
                }
            }
        }
    }
}
