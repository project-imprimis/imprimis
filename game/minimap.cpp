#include "game.h"

namespace game
{
    VARP(minimapminscale, 0, 256, 10000);
    VARP(minimapmaxscale, 1, 256, 10000);
    VARP(minimapshowteammates, 0, 1, 1);

    float calcradarscale()
    {
        //clamp minimapradius/3 to within min/max radar scale
        return std::clamp(max(minimapradius.x, minimapradius.y)/3, static_cast<float>(minimapminscale), static_cast<float>(minimapmaxscale));
    }

    void drawminimap(gameent *d, float x, float y, float s)
    {

        vec cameraoffset = vec(0,0,0).rotate_around_z(M_PI*game::player1->yaw/180);
        cameraoffset.x = -cameraoffset.x;
        cameraoffset = cameraoffset.add(game::player1->o);
        vec pos = cameraoffset.sub(minimapcenter).mul(minimapscale).add(vec(0.5f, 0.48f, 0.5f)),
            dir;
        vecfromyawpitch(0, 0, 1, 0, dir);
        float scale = calcradarscale();
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_FAN);
        for(int i = 0; i < 32; ++i)
        {
            vec v = vec(0, -1, 0).rotate_around_z(i/32.0f*2*M_PI);
            //gle::attribf(x + 0.5f*s*(1.0f + v.x), y + 0.5f*s*(1.0f + 1.3f/sqrt(1+ (2+v.y)*(2+v.y))*v.y));
            gle::attribf(x + 0.5f*s*(1.0f + v.x), y + 0.5f*s*(1.0f + 0.7f*v.y));
            vec tc = vec(dir).rotate_around_z(i/32.0f*2*M_PI);
            gle::attribf(1.0f - (pos.x + tc.x*scale*minimapscale.x), pos.y + tc.y*scale*minimapscale.y);
        }
        gle::end();
    }

    void setradartex()
    {
        settexture("media/interface/radar/radar.png", 3);
    }

    void drawteammate(gameent *d, float x, float y, float s, gameent *o, float scale, float blipsize = 1)
    {
        vec dir = d->o;
        dir.sub(o->o).div(scale);
        float dist = dir.magnitude2(),
              maxdist = 1 - 0.05f - 0.05f;
        if(dist >= maxdist)
        {
            dir.mul(maxdist/dist);
        }
        dir.rotate_around_z(-camera1->yaw/RAD);
        float bs = 0.06f*blipsize*s,
              bx = x + s*0.5f*(1.0f + dir.x),
              by = y + s*0.5f*(1.0f + dir.y);
        vec v(-0.5f, -0.5f, 0);
        v.rotate_around_z((90+o->yaw-camera1->yaw)/RAD);
        gle::attribf(bx + bs*v.x, by + bs*v.y); gle::attribf(0, 0);
        gle::attribf(bx + bs*v.y, by - bs*v.x); gle::attribf(1, 0);
        gle::attribf(bx - bs*v.x, by - bs*v.y); gle::attribf(1, 1);
        gle::attribf(bx - bs*v.y, by + bs*v.x); gle::attribf(0, 1);
    }

    void setbliptex(int team, const char *type = "")
    {
        DEF_FORMAT_STRING(blipname, "media/interface/radar/blip%s%s.png", teamblipcolor[validteam(team) ? team : 0], type);
        settexture(blipname, 3);
    }

    void drawplayerblip(gameent *d, float x, float y, float s, float blipsize)
    {
        if(d->state != ClientState_Alive && d->state != ClientState_Dead)
        {
            return;
        }
        float scale = calcradarscale();
        setbliptex(d->team, d->state == ClientState_Dead ? "_dead" : "_alive");
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_QUADS);
        drawteammate(d, x, y, s, d, scale, blipsize);
        gle::end();
    }

    void drawteammates(gameent *d, float x, float y, float s)
    {
        if(!minimapshowteammates)
        {
            return;
        }
        float scale = calcradarscale();
        int alive = 0, dead = 0;
        for(int i = 0; i < players.length(); i++)
        {
            gameent *o = players[i];
            if(o != d && o->state == ClientState_Alive && o->team == d->team)
            {
                if(!alive++)
                {
                    setbliptex(d->team, "_alive");
                    gle::defvertex(2);
                    gle::deftexcoord0();
                    gle::begin(GL_QUADS);
                }
                drawteammate(d, x, y, s, o, scale);
            }
        }
        if(alive)
        {
            gle::end();
        }
        for(int i = 0; i < players.length(); i++)
        {
            gameent *o = players[i];
            if(o != d && o->state == ClientState_Dead && o->team == d->team)
            {
                if(!dead++)
                {
                    setbliptex(d->team, "_dead");
                    gle::defvertex(2);
                    gle::deftexcoord0();
                    gle::begin(GL_QUADS);
                }
                drawteammate(d, x, y, s, o, scale);
            }
        }
        if(dead)
        {
            gle::end();
        }
    }

    FVARP(minimapalpha, 0, 1, 1);

    VAR(mmaps, 0, 40, 100);
    VAR(mmapw, 0, 50, 100);
    VAR(mmaph, 0, 72, 100);

    void drawradar(float x, float y, float s)
    {
        gle::defvertex(2);
        gle::deftexcoord0();
        gle::begin(GL_TRIANGLE_STRIP);
        gle::attribf(x,   y);   gle::attribf(0, 0);
        gle::attribf(x+s, y);   gle::attribf(1, 0);
        gle::attribf(x,   y+s); gle::attribf(0, 1);
        gle::attribf(x+s, y+s); gle::attribf(1, 1);
        gle::end();
    }

    void drawhud(gameent *d, int x, int y, int s)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        gle::colorf(1, 1, 1, minimapalpha);
        if(minimapalpha >= 1)
        {
            glDisable(GL_BLEND);
        }
        bindminimap();
        drawminimap(d, x, y, s);
        if(minimapalpha >= 1)
        {
            glEnable(GL_BLEND);
        }
        gle::colorf(1, 1, 1);
        float margin = 0.04f,
              roffset = s*margin,
              rsize = s + 2*roffset;
        pushhudmatrix();
        hudmatrix.translate(x - roffset + 0.5f*rsize, y - roffset + 0.5f*rsize, 0);
        hudmatrix.rotate_around_z((camera1->yaw + 180)/-RAD);
        flushhudmatrix();
        pophudmatrix();
        drawplayerblip(d, x, y+50, s, 1.5f);
    }

    void renderhud()
    {
        int size         = mmaps*desktoph/100, // mmaps % of total screen size
            widthoffset  = mmapw*(desktopw-size)/100,
            heightoffset = desktoph*mmaph/100;
        drawhud(hudplayer(), widthoffset, heightoffset, size);
    }

    void updateminimap()
    {
        vec cameraoffset = vec(0,0,190);//.rotate_around_z(M_PI*game::player1->yaw/180);
        cameraoffset = cameraoffset.add(game::player1->o);
        drawminimap(game::player1->yaw, -42, cameraoffset, rootworld, 2);
    }
}
