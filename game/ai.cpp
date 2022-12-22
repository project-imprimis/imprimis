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

    void avoid()
    {
        // guess as to the radius of ai and other critters relying on the avoid set for now
        float guessradius = player1->radius;
        obstacles.clear();
        for(uint i = 0; i < players.size(); i++)
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
            for(uint i = 0; i < players.size(); i++)
            {
                if(players[i]->ai)
                {
                    total++;
                }
            }
            for(uint i = 0; i < players.size(); i++)
            {
                if(players[i]->state == ClientState_Alive && players[i]->ai)
                {
                    gameent *d = players[i];
                    vec pos = d->abovehead();
                    pos.z += 3;
                    alive++;
                    if(aidebug >= 4)
                    {
                        /* need to dynamic_cast down to waypointai, which extends
                         * aiinfo with a waypoint type implementation
                         *
                         * required since render() needs information about the ai
                         * which is not required for most other uses of the aiinfo
                         * object
                         */
                        waypointai * wpai = dynamic_cast<waypointai *>(d->ai);
                        wpai->drawroute(4.f*(static_cast<float>(alive)/static_cast<float>(total)));
                    }
                    if(aidebug >= 3)
                    {
                        waypointai * wpai = dynamic_cast<waypointai *>(d->ai);
                        DEF_FORMAT_STRING(q, "node: %d route: %d (%d)",
                            d->lastnode,
                            !wpai->route.empty() ? wpai->route[0] : -1,
                            static_cast<int>(wpai->route.size())
                        );
                        pos.z += 2;
                    }
                    bool top = true;
                    for(int i = static_cast<int>(d->ai->state.size()); --i >=0;) //note reverse iteration
                    {
                        aistate &b = d->ai->state[i];
                        DEF_FORMAT_STRING(s, "%s%s (%d ms) %s:%d",
                            top ? "^fg" : "^fy",
                            stnames[b.type],
                            lastmillis-b.millis,
                            sttypes[b.targtype+1], b.target
                        );
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
                            pos.z += 2;
                        }
                        gameent *e = getclient(d->ai->enemy);
                        if(e)
                        {
                            pos.z += 2;
                        }
                    }
                }
            }
            if(aidebug >= 4)
            {
                int cur = 0;
                for(const avoidset::obstacle& ob : obstacles.obstacles)
                {
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
            std::vector<int> close;
            size_t len = waypoints.size();
            if(showwaypointsradius)
            {
                findwaypointswithin(camera1->o, 0, showwaypointsradius, close);
                len = close.size();
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
