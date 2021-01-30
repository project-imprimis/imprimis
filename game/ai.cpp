#include "game.h"

namespace ai
{
    using namespace game;

    avoidset obstacles;
    vec aitarget(0, 0, 0);

    VAR(aidebug, 0, 0, 6);

    ICOMMAND(addbot, "s", (char *s), addmsg(NetMsg_AddBot, "ri", *s ? std::clamp(parseint(s), 1, 101) : -1));
    ICOMMAND(delbot, "", (), addmsg(NetMsg_DelBot, "r"));
    ICOMMAND(botlimit, "i", (int *n), addmsg(NetMsg_BotLimit, "ri", *n));
    ICOMMAND(botbalance, "i", (int *n), addmsg(NetMsg_BotBalance, "ri", *n));

    bool makeroute(gameent *d, aistate &b, int node, bool changed, int retries)
    {
        if(!iswaypoint(d->lastnode))
        {
            return false;
        }
        if(changed && d->ai->route.length() > 1 && d->ai->route[0] == node)
        {
            return true;
        }
        if(route(d, d->lastnode, node, d->ai->route, obstacles, retries))
        {
            b.override = false;
            return true;
        }
        // retry fails: 0 = first attempt, 1 = try ignoring obstacles, 2 = try ignoring prevnodes too
        if(retries <= 1)
        {
            return makeroute(d, b, node, false, retries+1);
        }
        return false;
    }

    bool makeroute(gameent *d, aistate &b, const vec &pos, bool changed, int retries)
    {
        int node = closestwaypoint(pos, sightmin, true);
        return makeroute(d, b, node, changed, retries);
    }

    static vector<int> targets;

    bool checkroute(gameent *d, int n)
    {
        if(d->ai->route.empty() || !d->ai->route.inrange(n))
        {
            return false;
        }
        int last = d->ai->lastcheck ? lastmillis-d->ai->lastcheck : 0;
        if(last < 500 || n < 3)
        {
            return false; // route length is too short
        }
        d->ai->lastcheck = lastmillis;
        int w = iswaypoint(d->lastnode) ? d->lastnode : d->ai->route[n], c = min(n-1, numprevnodes);
        // check ahead to see if we need to go around something
        for(int j = 0; j < c; ++j)
        {
            int p = n-j-1,
                v = d->ai->route[p];
            if(d->ai->hasprevnode(v) || obstacles.find(v, d)) // something is in the way, try to remap around it
            {
                int m = p-1;
                if(m < 3)
                {
                    return false; // route length is too short from this point
                }
                for(int i = m; --i >= 0;) //note reverse iteration
                {
                    int t = d->ai->route[i];
                    if(!d->ai->hasprevnode(t) && !obstacles.find(t, d))
                    {
                        static vector<int> remap; remap.setsize(0);
                        if(route(d, w, t, remap, obstacles))
                        { // kill what we don't want and put the remap in
                            while(d->ai->route.length() > i)
                            {
                                d->ai->route.pop();
                            }
                            for(int k = 0; k < remap.length(); k++)
                            {
                                d->ai->route.add(remap[k]);
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

    void avoid()
    {
        // guess as to the radius of ai and other critters relying on the avoid set for now
        float guessradius = player1->radius;
        obstacles.clear();
        for(int i = 0; i < players.length(); i++)
        {
            dynent *d = players[i];
            if(d->state != ClientState_Alive)
            {
                continue;
            }
            obstacles.avoidnear(d, d->o.z + d->aboveeye + 1, d->feetpos(), guessradius + d->radius);
        }
        extern avoidset wpavoid;
        obstacles.add(wpavoid);
        avoidweapons(obstacles, guessradius);
    }

    void drawroute(gameent *d, float amt)
    {
        int last = -1;
        for(int i = d->ai->route.length(); --i >=0;) //note reverse iteration
        {
            if(d->ai->route.inrange(last))
            {
                int index = d->ai->route[i],
                    prev = d->ai->route[last];
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
            vec pos = d->feetpos();
            if(d->ai->spot != vec(0, 0, 0))
            {
                particle_flare(pos, d->ai->spot, 1, Part_Streak, 0x00FFFF);
            }
            if(iswaypoint(d->ai->targnode))
            {
                particle_flare(pos, waypoints[d->ai->targnode].o, 1, Part_Streak, 0xFF00FF);
            }
            if(iswaypoint(d->lastnode))
            {
                particle_flare(pos, waypoints[d->lastnode].o, 1, Part_Streak, 0xFFFF00);
            }
            for(int i = 0; i < numprevnodes; ++i)
            {
                if(iswaypoint(d->ai->prevnodes[i]))
                {
                    particle_flare(pos, waypoints[d->ai->prevnodes[i]].o, 1, Part_Streak, 0x884400);
                    pos = waypoints[d->ai->prevnodes[i]].o;
                }
            }
        }
    }

    VAR(showwaypoints, 0, 0, 1); //display waypoint locations in edit mode
    VAR(showwaypointsradius, 0, 200, 10000); //maximum distance to display (200 = 25m)

    const char *stnames[AIState_Max]    = { "wait", "defend", "pursue"},
               *sttypes[AITravel_Max+1] = { "none", "node", "player", "entity" };
    void render()
    {
        if(aidebug > 1)
        {
            int total = 0,
                alive = 0;
            for(int i = 0; i < players.length(); i++)
            {
                if(players[i]->ai)
                {
                    total++;
                }
            }
            for(int i = 0; i < players.length(); i++)
            {
                if(players[i]->state == ClientState_Alive && players[i]->ai)
                {
                    gameent *d = players[i];
                    vec pos = d->abovehead();
                    pos.z += 3;
                    alive++;
                    if(aidebug >= 4)
                    {
                        drawroute(d, 4.f*(static_cast<float>(alive)/static_cast<float>(total)));
                    }
                    if(aidebug >= 3)
                    {
                        DEF_FORMAT_STRING(q, "node: %d route: %d (%d)",
                            d->lastnode,
                            !d->ai->route.empty() ? d->ai->route[0] : -1,
                            d->ai->route.length()
                        );
                        particle_textcopy(pos, q, Part_Text, 1);
                        pos.z += 2;
                    }
                    bool top = true;
                    for(int i = d->ai->state.length(); --i >=0;) //note reverse iteration
                    {
                        aistate &b = d->ai->state[i];
                        DEF_FORMAT_STRING(s, "%s%s (%d ms) %s:%d",
                            top ? "\fg" : "\fy",
                            stnames[b.type],
                            lastmillis-b.millis,
                            sttypes[b.targtype+1], b.target
                        );
                        particle_textcopy(pos, s, Part_Text, 1);
                        pos.z += 2;
                        if(top)
                        {
                            if(aidebug >= 3)
                            {
                                 top = false;
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    if(aidebug >= 3)
                    {
                        if(d->ai->weappref >= 0 && d->ai->weappref < Gun_NumGuns)
                        {
                            particle_textcopy(pos, guns[d->ai->weappref].name, Part_Text, 1);
                            pos.z += 2;
                        }
                        gameent *e = getclient(d->ai->enemy);
                        if(e)
                        {
                            particle_textcopy(pos, colorname(e), Part_Text, 1);
                            pos.z += 2;
                        }
                    }
                }
            }
            if(aidebug >= 4)
            {
                int cur = 0;
                for(int i = 0; i < obstacles.obstacles.length(); i++)
                {
                    const avoidset::obstacle &ob = obstacles.obstacles[i];
                    int next = cur + ob.numwaypoints;
                    for(; cur < next; cur++)
                    {
                        int ent = obstacles.waypoints[cur];
                        if(iswaypoint(ent))
                        {
                            regular_particle_splash(Part_Edit, 2, 40, waypoints[ent].o, 0xFF6600, 1.5f);
                        }
                    }
                    cur = next;
                }
            }
        }
        if(showwaypoints || aidebug >= 6)
        {
            vector<int> close;
            int len = waypoints.length();
            if(showwaypointsradius)
            {
                findwaypointswithin(camera1->o, 0, showwaypointsradius, close);
                len = close.length();
            }
            for(int i = 0; i < len; ++i)
            {
                waypoint &w = waypoints[showwaypointsradius ? close[i] : i];
                for(int j = 0; j < maxwaypointlinks; ++j)
                {
                    int link = w.links[j];
                    if(!link)
                    {
                        break;
                    }
                    particle_flare(w.o, waypoints[link].o, 1, Part_Streak, 0x0000FF);
                }
            }
        }
    }
}
