// creation of scoreboard
#include "game.h"

namespace game
{
    VARP(showservinfo, 0, 1, 1);
    VARP(showclientnum, 0, 0, 1);
    VARP(showpj, 0, 0, 1);
    VARP(showping, 0, 1, 1);
    VARP(showspectators, 0, 1, 1);
    VARP(highlightscore, 0, 1, 1);
    VARP(showconnecting, 0, 0, 1);
    VARP(showfrags, 0, 1, 1);

    teaminfo teaminfos[maxteams];

    void clearteaminfo()
    {
        for(int i = 0; i < maxteams; ++i)
        {
            teaminfos[i].reset();
        }
    }

    void setteaminfo(int team, int frags)
    {
        if(!validteam(team))
        {
            return;
        }
        teaminfo &t = teaminfos[team-1];
        t.frags = frags;
    }

    //orders a pair of players' rank according to different criteria depending on mode
    //spectators are ordered alphabetically
    //ctf players are ordered by captures
    //non-ctf is ordered by kills
    static inline bool playersort(const gameent *a, const gameent *b)
    {
        if(a->state==ClientState_Spectator)
        {
            if(b->state==ClientState_Spectator)
            {
                return strcmp(a->name, b->name) < 0;
            }
            else
            {
                return false;
            }
        }
        else if(b->state==ClientState_Spectator)
        {
            return true;
        }
        if(a->score > b->score)
        {
            return true;
        }
        if(a->score < b->score)
        {
            return false;
        }
        return strcmp(a->name, b->name) < 0;
    }

    //takes a vector of gameents and returns the one with the most kills
    void getbestplayers(std::vector<gameent *> &best)
    {
        for(uint i = 0; i < players.size(); i++)
        {
            gameent *o = players[i];
            if(o->state!=ClientState_Spectator)
            {
                best.push_back(o);
            }
        }
        std::sort(best.begin(), best.end(), playersort);
        while(best.size() > 1 && best.back()->frags < best[0]->frags)
        {
            best.pop_back();
        }
    }

    //taes a vector of team numbers and returns the one with the most frags
    void getbestteams(std::vector<int> &best)
    {
        if(cmode && cmode->hidefrags())
        {
            std::vector<teamscore> teamscores;
            cmode->getteamscores(teamscores);
            std::sort(teamscores.begin(), teamscores.end(), teamscore::compare);
            //loop through and drop teams
            while(teamscores.size() && teamscores.back().score < teamscores[0].score)
            {
                teamscores.pop_back();
            }
            for(uint i = 0; i < teamscores.size(); i++)
            {
                best.push_back(teamscores[i].team);
            }
        }
        else
        {
            int bestfrags = INT_MIN;
            for(int i = 0; i < maxteams; ++i)
            {
                teaminfo &t = teaminfos[i];
                bestfrags = max(bestfrags, t.frags);
            }
            for(int i = 0; i < maxteams; ++i)
            {
                teaminfo &t = teaminfos[i];
                if(t.frags >= bestfrags)
                {
                    best.push_back(1+i);
                }
            }
        }
    }

    static std::vector<gameent *> teamplayers[1+maxteams], spectators;

    static void groupplayers()
    {
        for(int i = 0; i < 1+maxteams; ++i)
        {
            teamplayers[i].clear();
        }
        spectators.clear();
        for(uint i = 0; i < players.size(); i++)
        {
            gameent *o = players[i];
            if(!showconnecting && !o->name[0])
            {
                continue;
            }
            if(o->state==ClientState_Spectator)
            {
                spectators.push_back(o);
                continue;
            }
            int team = modecheck(gamemode, Mode_Team) && validteam(o->team) ? o->team : 0;
            teamplayers[team].push_back(o);
        }
        for(int i = 0; i < 1+maxteams; ++i)
        {
            std::sort(teamplayers[i].begin(), teamplayers[i].end(), playersort);
        }
        std::sort(spectators.begin(), spectators.end(), playersort);
    }

    void removegroupedplayer(gameent *d)
    {
        for(int i = 0; i < 1+maxteams; ++i)
        {
            auto itr = std::find(teamplayers[i].begin(), teamplayers[i].end(), d);
            if(itr != teamplayers[i].end())
            {
                teamplayers[i].erase(itr);
            }
        }
        auto itr = std::find(spectators.begin(), spectators.end(), d);
        if(itr != spectators.end())
        {
            spectators.erase(itr);
        }
    }

    void refreshscoreboard()
    {
        groupplayers();
    }
//scoreboard commands
    COMMAND(refreshscoreboard, "");
    void numscoreboard(int *team)
    {
        intret(*team < 0 ? spectators.size() : (*team <= maxteams ? teamplayers[*team].size() : 0));
    }
    COMMAND(numscoreboard, "i");
    void loopscoreboard(ident *id, int *team, uint *body)
    {
        if(*team > maxteams)
        {
            return;
        }
        LOOP_START(id, stack);
        std::vector<gameent *> &p = *team < 0 ? spectators : teamplayers[*team];
        for(size_t i = 0; i < p.size(); i++)
        {
            loopiter(id, stack, p[i]->clientnum);
            execute(body);
        }
        loopend(id, stack);
    }
    COMMAND(loopscoreboard, "rie");

    void scoreboardstatus(int *cn)
    {
        gameent *d = getclient(*cn);
        if(d)
        {
            int status = d->state!=ClientState_Dead ? 0xFFFFFF : 0x606060;
            if(d->privilege)
            {
                status = d->privilege>=Priv_Admin ? 0xFF8000 : 0x40FF80;
                if(d->state==ClientState_Dead)
                {
                    status = (status>>1)&0x7F7F7F;
                }
            }
            intret(status);
        }
    }
    COMMAND(scoreboardstatus, "i");
    //scoreboard packet jump
    void scoreboardpj (int *cn)
    {
        gameent *d = getclient(*cn);
        if(d && d != player1)
        {
            if(d->state==ClientState_Lagged)
            {
                result("LAG");
            }
            else
            {
                intret(d->plag);
            }
        }
    }
    COMMAND(scoreboardpj, "i");

    void scoreboardping(int *cn)
    {
        gameent *d = getclient(*cn);
        if(d)
        {
            if(!showpj && d->state==ClientState_Lagged)
            {
                result("LAG");
            }
            else
            {
                intret(d->ping);
            }
        }
    }
    COMMAND(scoreboardping, "i");
//scoreboard booleans
    void scoreboardshowfrags()
    {
        intret(cmode && cmode->hidefrags() && !showfrags ? 0 : 1);
    }
    COMMAND(scoreboardshowfrags, "");
    void scoreboardshowclientnum()
    {
        intret(showclientnum || player1->privilege>=Priv_Master ? 1 : 0);
    }
    COMMAND(scoreboardshowclientnum, "");
    void scoreboardmultiplayer()
    {
        intret(multiplayer || demoplayback ? 1 : 0);
    }
    COMMAND(scoreboardmultiplayer, "");

    void scoreboardhighlight(int *cn)
    {
        intret(*cn == player1->clientnum && highlightscore && (multiplayer || demoplayback || players.size() > 1) ? 0x808080 : 0);
    }
    COMMAND(scoreboardhighlight, "i");

    void scoreboardservinfo()
    {
        if(!showservinfo)
        {
            return;
        }
        const ENetAddress *address = connectedpeer();
        if(address && player1->clientnum >= 0)
        {
            if(servdesc[0])
            {
                result(servdesc);
            }
            else
            {
                string hostname;
                if(enet_address_get_host_ip(address, hostname, sizeof(hostname)) >= 0)
                {
                    result(tempformatstring("%s:%d", hostname, address->port));
                }
            }
        }
    }
    COMMAND(scoreboardservinfo, "");

    void scoreboardmode()
    {
        result(server::modeprettyname(gamemode));
    }
    COMMAND(scoreboardmode, "");

    void scoreboardmap()
    {
        const char *mname = getclientmap();
        result(mname[0] ? mname : "[new map]");
    }
    COMMAND(scoreboardmap, "");

    void scoreboardtime()
    {
        if(!modecheck(gamemode, Mode_Untimed) && getclientmap() && (maplimit >= 0 || intermission))
        {
            if(intermission)
            {
                result("intermission");
            }
            else
            {
                int secs = max(maplimit-lastmillis, 0)/1000;
                result(tempformatstring("%d:%02d", secs/60, secs%60));
            }
        }
    }
    COMMAND(scoreboardtime, "");

    void getteamscore(int *team)
    {
        if(modecheck(gamemode, Mode_Team) && validteam(*team))
        {
            if(cmode && cmode->hidefrags())
            {
                intret(cmode->getteamscore(*team));
            }
            else
            {
                intret(teaminfos[*team-1].score);
            }
        }
    }
    COMMAND(getteamscore, "i");

    void showscores(bool on)
    {
        UI::holdui("scoreboard", on);
    }
}
