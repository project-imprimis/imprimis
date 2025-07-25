// weapon.cpp: all shooting and effects code, projectile management
#include "game.h"
#include "sound.h"
#include <random>

namespace game
{
    static const int offsetmillis = 500;
    static const int projdepth = 2; //depth to have projectile die at upon hitting cube geometry
    vec rays[MAXRAYS];

    struct hitmsg
    {
        int target, lifesequence, info1, info2;
        ivec dir;
    };
    static std::vector<hitmsg> hits;

/*getweapon
 *returns the index of the weapon in hand
 * Arguments:
 *  none
 * Returns:
 *  index of weapon (zero-indexed)
 */
    ICOMMAND(getweapon, "", (), intret(player1->gunselect));


/*getweaponheat
 * returns the heat amount of the weapon, between 0 and 1
 *
 */
    void getweaponheat()
    {
        int gun = player1->gunselect,
            atk = guns[gun].attacks[1];
        if(player1->heat[gun] > attacks[atk].maxheat) // check if weapon has overheated
        {
            floatret(0.99f);
        }
        else
        {
            floatret(static_cast<float>(player1->heat[gun]) / attacks[atk].maxheat);
        }
    }
    COMMAND(getweaponheat, "");

    VARF(spawncombatclass, 0, 0, 3,
    {
        checkclass();
    });

    //sets spawncombatclass to a valid class for the team
    void checkclass()
    {
        if(player1->team == 1)
        {
            if(spawncombatclass > 1)
            {
                spawncombatclass = 0; //don't allow wrong class
            }
        }
        else if(player1->team == 2)
        {
            if(spawncombatclass <= 1)
            {
                spawncombatclass = 3; //don't allow wrong class
            }
        }
    }

/*getcombatclass
 * returns the combat class the player currently has (may be different than
 * `spawncombatclass` as `spawncombatclass` can be changed while the player
 * is still alive)
 *
 * Arguments:
 *   none
 * Returns:
 *  (to cubescript) player character's current combat class
 */
    void getcombatclass()
    {
        intret(player1->combatclass);
    }
    COMMAND(getcombatclass, "");

    void gunselect(int gun, gameent *d)
    {
        if(gun!=d->gunselect)
        {
            addmsg(NetMsg_GunSelect, "rci", d, gun);
            soundmain.playsound(Sound_WeapLoad, d == player1 ? nullptr : &d->o);
        }
        d->gunselect = gun;
    }

    bool weaponallowed(int weapon, gameent * player)
    {
        //rifle
        if(player->combatclass == 0)
        {
            switch(weapon)
            {
                case Gun_Rail:
                case Gun_Carbine:
                    return true;
                default:
                    return false;
            }
        }
        //demo
        else if(player->combatclass == 1)
        {
            switch(weapon)
            {
                case Gun_Pulse:
                case Gun_Carbine:
                    return true;
                default:
                    return false;
            }
        }
        //eng
        else if(player->combatclass == 2)
        {
            switch(weapon)
            {
                case Gun_Eng:
                case Gun_Carbine:
                    return true;
                default:
                    return false;
            }
        }
        //shotgun
        else if(player->combatclass == 3)
        {
            switch(weapon)
            {
                case Gun_Shotgun:
                case Gun_Carbine:
                    return true;
                default:
                    return false;
            }
        }
        else
        {
            return false;
        }
    }


/*nextweapon
 *changes player to an adjacent weapon, forwards if no dir is passed
 * Arguments:
 *  int *dir: direction (backwards if negative, forwards if positive)
 * Returns:
 *  void
 */
    static void nextweapon(int dir)
    {
        if(player1->state!=ClientState_Alive)
        {
            return;
        }
        dir = dir ? 1 : -1;
        for(uint i = 1; i < Gun_NumGuns; ++i)
        {
            int gun = (player1->gunselect + dir*i)%Gun_NumGuns;
            if(weaponallowed(gun))
            {
                gunselect(gun, player1);
                break;
            }
        }
    }
    ICOMMAND(nextweapon, "i", (int *dir), nextweapon(*dir));

    int getweapon(const char *name)
    {
        if(isdigit(name[0]))
        {
            return parseint(name);
        }
        else
        {
            int len = strlen(name);
            for(int i = 0; i < static_cast<int>(sizeof(guns)/sizeof(guns[0])); ++i)
            {
                if(!strncasecmp(guns[i].name, name, len))
                {
                    return i;
                }
            }
        }
        return -1;
    }

    void setweapon(const char *name, bool force)
    {
        int gun = getweapon(name);
        if(player1->state!=ClientState_Alive || !validgun(gun))
        {
            return;
        }
        if(!weaponallowed(gun))
        {
            return;
        }
        gunselect(gun, player1);
    }
    ICOMMAND(setweapon, "si", (char *name, int *force), setweapon(name, *force!=0));

    void primaryweapon()
    {
        if(weaponallowed(Gun_Rail))
        {
            gunselect(Gun_Rail, player1);
        }
        else if(weaponallowed(Gun_Pulse))
        {
            gunselect(Gun_Pulse, player1);
        }
        else if(weaponallowed(Gun_Eng))
        {
            gunselect(Gun_Eng, player1);
        }
        else if(weaponallowed(Gun_Shotgun))
        {
            gunselect(Gun_Shotgun, player1);
        }
        else
        {
            gunselect(Gun_Shotgun, player1);
        }
    }
    COMMAND(primaryweapon, "");

    //selects for player1 the secondary weapon for their class, which is currently
    //always carbine
    void secondaryweapon()
    {
        gunselect(Gun_Carbine, player1);
    }
    COMMAND(secondaryweapon, "");

    void cycleweapon(int numguns, int *guns, bool force = false)
    {
        if(numguns<=0 || player1->state!=ClientState_Alive)
        {
            return;
        }
        int offset = 0;
        for(int i = 0; i < numguns; ++i)
        {
            if(guns[i] == player1->gunselect)
            {
                offset = i+1;
                break;
            }
        }
        for(int i = 0; i < numguns; ++i)
        {
            int gun = guns[(i+offset)%numguns];
            if(gun>=0 && gun<Gun_NumGuns && (force || player1->ammo[gun]))
            {
                gunselect(gun, player1);
                return;
            }
        }
    }

    void weaponswitch(gameent *d)
    {
        if(d->state!=ClientState_Alive)
        {
            return;
        }
        int s = d->gunselect;
        if(s!=Gun_Pulse && d->ammo[Gun_Pulse])
        {
            s = Gun_Pulse;
        }
        else if(s!=Gun_Rail && d->ammo[Gun_Rail])
        {
            s = Gun_Rail;
        }
        gunselect(s, d);
    }

    //gaussian spread, weighted average 10:0:1
    void offsetray(const vec &from, const vec &to, int spread, float range, vec &dest, bool wander = true)
    {
        static vec old(0,0);
        std::random_device rd{};
        std::mt19937 gen{rd()};

        std::normal_distribution<float> d{0, 1.5};
        vec offset = vec( d(gen), d(gen), d(gen) );                             //generate a 3-vector, stdev +/- 1.5 normal dist
        if(wander)
        {
            old.mul(10).add(offset).add(vec(0,0,0)).div(11);                    //weight average, 10x for previous, 1x for 0 (centering), 1x for new random
            offset = vec(old).rotate_around_z(camera1->yaw/RAD);                //old value is not normalized to camera orientation
        }
        offset.mul((to.dist(from)/1024)*spread);                                //offset weighted by projectile distance
        dest = vec(offset).add(to);                                             //apply offset
        if(dest != from)
        {
            vec dir = vec(dest).sub(from).normalize();
            raycubepos(from, dir, dest, range, Ray_ClipMat|Ray_AlphaPoly);
        }
    }

    void createrays(int atk, const vec &from, const vec &to)             // create random spread of rays
    {
        for(int i = 0; i < attacks[atk].rays; ++i)
        {
            offsetray(from, to, attacks[atk].spread, attacks[atk].time, rays[i]);
        }
    }

    enum
    {
        Bouncer_Gibs,
        Bouncer_Debris
    };

    struct bouncer : physent
    {
        int lifetime, bounces;
        float lastyaw, roll;
        bool local;
        const gameent *owner;
        int bouncetype, variant;
        vec offset;
        int offsetmillis;
        int id;

        bouncer() : bounces(0), roll(0), variant(0)
        {
            type = physent::PhysEnt_Bounce;
        }
    };

    static std::vector<bouncer> bouncers;

    void newbouncer(const vec &from, const vec &to, bool local, int id, const gameent *owner, int type, int lifetime, int speed)
    {
        bouncers.emplace_back();
        bouncer &bnc = bouncers.back();
        bnc.o = from;
        bnc.radius = bnc.xradius = bnc.yradius = type==Bouncer_Debris ? 0.5f : 1.5f;
        bnc.eyeheight = bnc.radius;
        bnc.aboveeye = bnc.radius;
        bnc.lifetime = lifetime;
        bnc.local = local;
        bnc.owner = owner;
        bnc.bouncetype = type;
        bnc.id = local ? lastmillis : id;

        switch(type)
        {
            case Bouncer_Debris:
            {
                bnc.variant = randomint(4);
                break;
            }
            case Bouncer_Gibs:
            {
                bnc.variant = randomint(3);
                break;
            }
        }

        vec dir(to);
        dir.sub(from).safenormalize();
        bnc.vel = dir;
        bnc.vel.mul(speed);

        avoidcollision(&bnc, dir, owner, 0.1f);

        bnc.offset = from;
        bnc.offset.sub(bnc.o);
        bnc.offsetmillis = offsetmillis;

        bnc.resetinterp();
    }

    void bounced(bouncer &b, const vec &surface)
    {
        if(b.type != physent::PhysEnt_Bounce)
        {
            return;
        }
        if(b.bouncetype != Bouncer_Gibs || b.bounces >= 2)
        {
            return;
        }
        b.bounces++;
        addstain(Stain_Blood, vec(b.o).sub(vec(surface).mul(b.radius)), surface, 2.96f/b.bounces, bvec(0x60, 0xFF, 0xFF), randomint(4));
    }

    bool bounce(bouncer &b, float secs, float elasticity, float waterfric, float grav)
    {
        vec cwall(0,0,0);
        // make sure bouncers don't start inside geometry
        if(b.physstate!=PhysEntState_Bounce && collide(&b, &cwall, vec(0, 0, 0), 0, false))
        {
            return true;
        }
        int mat = rootworld.lookupmaterial(vec(b.o.x, b.o.y, b.o.z + (b.aboveeye - b.eyeheight)/2));
        bool water = IS_LIQUID(mat);
        if(water)
        {
            b.vel.z -= grav*gravity/16*secs;
            b.vel.mul(max(1.0f - secs/waterfric, 0.0f));
        }
        else
        {
            b.vel.z -= grav*gravity*secs;
        }
        vec old(b.o);
        for(int i = 0; i < 2; ++i)
        {
            vec dir(b.vel);
            dir.mul(secs);
            b.o.add(dir);
            if(!collide(&b, nullptr, dir, 0, true))
            {
                if(collideinside)
                {
                    b.o = old;
                    b.vel.mul(-elasticity);
                }
                break;
            }
            else if(collideplayer)
            {
                break;
            }
            b.o = old;
            game::bounced(b, cwall);
            float c = cwall.dot(b.vel),
                  k = 1.0f + (1.0f-elasticity)*c/b.vel.magnitude();
            b.vel.mul(k);
            b.vel.sub(vec(cwall).mul(elasticity*2.0f*c));
        }
        if(b.physstate!=PhysEntState_Bounce)
        {
            // make sure bouncers don't start inside geometry
            if(b.o == old)
            {
                return !collideplayer;
            }
            b.physstate = PhysEntState_Bounce;
        }
        return collideplayer!=nullptr;
    }

    void updatebouncers(int time)
    {
        for(uint i = 0; i < bouncers.size(); i++) //∀ bouncers currently in the game
        {
            bouncer &bnc = bouncers[i];
            vec old(bnc.o);
            bool stopped = false;
            // cheaper variable rate physics for debris, gibs, etc.
            for(int rtime = time; rtime > 0;)
            {
                int qtime = min(30, rtime);
                rtime -= qtime;
                //if bouncer has run out of lifetime, or bounce fxn returns true, turn on the stopping flag
                if((bnc.lifetime -= qtime)<0 || bounce(bnc, qtime/1000.0f, 0.6f, 0.5f, 1))
                {
                    stopped = true;
                    break;
                }
            }
            if(stopped) //kill bouncer object if above check passes
            {
                bouncers.erase(bouncers.begin() + i);
                i--;
            }
            else //time evolution
            {
                bnc.roll += old.sub(bnc.o).magnitude()/(4/RAD);
                bnc.offsetmillis = max(bnc.offsetmillis-time, 0);
            }
        }
    }

    void removebouncers(const gameent *owner)
    {
        for(uint i = 0; i < bouncers.size(); i++)
        {
            if(bouncers[i].owner==owner)
            {
                bouncers.erase(bouncers.begin() + i);
                i--;
            }
        }
    }

    void clearbouncers()
    {
        bouncers.clear();
    }

    struct projectile
    {
        vec dir, o, from, offset;
        float speed;
        gameent *owner;
        int atk;
        bool local;
        int offsetmillis;
        int id;
        int spawntime;
        int deathtime;
        int gravity;
    };
    std::vector<projectile> projs;

    void clearprojectiles()
    {
        projs.clear();
    }

    void newprojectile(const vec &from, const vec &to, float speed, bool local, int id, gameent *owner, int atk, int lifetime, int pgravity)
    {
        projs.emplace_back();
        projectile &p = projs.back();
        p.dir = vec(to).sub(from).safenormalize();
        p.o = from;
        p.from = from;
        p.offset = hudgunorigin(from, to, owner);
        p.offset.sub(from);
        p.speed = speed;
        p.local = local;
        p.owner = owner;
        p.atk = atk;
        p.offsetmillis = offsetmillis;
        p.id = local ? lastmillis : id;
        p.spawntime = lastmillis;
        p.deathtime = lastmillis+lifetime;
        p.gravity = pgravity;
    }

    void removeprojectiles(const gameent *owner)
    {
        int len = projs.size();
        for(int i = 0; i < len; ++i)
        {
            if(projs[i].owner==owner)
            {
                projs.erase(projs.begin() + i);
                i--;
                len--;
            }
        }
    }

    VARP(blood, 0, 1, 1);

    void damageeffect(int damage, const gameent *d, bool thirdperson)
    {
        vec p = d->o;
        p.z += 0.6f*(d->eyeheight + d->aboveeye) - d->eyeheight;
        if(blood)
        {
            particle_splash(Part_Blood, max(damage/10, randomint(3)+1), 1000, p, 0x60FFFF, 2.96f);
        }
    }

    void spawnbouncer(const vec &p, const vec &vel, const gameent *d, int type)
    {
        vec to(randomint(100)-50, randomint(100)-50, randomint(100)-50); //x,y,z = [-50,50] to get enough steps to create a good random vector
        if(to.iszero())
        {
            to.z += 1; //if all three are zero (bad luck!), set vector to (0,0,1)
        }
        to.normalize(); //smash magnitude back to 1
        to.add(p); //add this random to input &p
        //newbouncer( from, to, local, id, owner,  type,        lifetime,             speed)
        newbouncer(   p,    to,  true,  0,  d,     type, randomint(1000)+1000, randomint(100)+20);
    }

    void hit(int damage, dynent *d, gameent *at, const vec &vel, int atk, float info1, int info2 = 1)
    {
        if(at==player1 && d!=at)
        {
            if(hitsound && lasthit != lastmillis)
            {
                soundmain.playsound(Sound_Hit);
            }
            lasthit = lastmillis;
        }

        gameent *f = (gameent *)d;

        f->lastpain = lastmillis;
        if(at->type==physent::PhysEnt_Player && at->team != f->team)
        {
            at->totaldamage += damage;
        }

        if(modecheck(gamemode, Mode_LocalOnly) || f==at)
        {
            f->hitpush(damage, vel, at, atk);
        }

        if(modecheck(gamemode, Mode_LocalOnly))
        {
            damaged(damage, f, at);
        }
        else
        {
            hits.emplace_back();
            hitmsg &h = hits.back();
            h.target = f->clientnum;
            h.lifesequence = f->lifesequence;
            h.info1 = static_cast<int>(info1*DMF);
            h.info2 = info2;
            h.dir = f==at ? ivec(0, 0, 0) : ivec(vec(vel).mul(DNF));
            if(at==player1)
            {
                damageeffect(damage, f);
                if(f==player1)
                {
                    damageblend(damage);
                    damagecompass(damage, at ? at->o : f->o);
                }
            }
        }
    }

    void hitpush(int damage, dynent *d, gameent *at, vec &from, vec &to, int atk, int rays)
    {
        hit(damage, d, at, vec(to).sub(from).safenormalize(), atk, from.dist(to), rays);
    }

    float projdist(dynent *o, vec &dir, const vec &v, const vec &vel)
    {
        vec middle = o->o;
        middle.z += (o->aboveeye-o->eyeheight)/2;
        dir = vec(middle).sub(v).add(vec(vel).mul(5)).safenormalize();

        float low = min(o->o.z - o->eyeheight + o->radius, middle.z),
              high = max(o->o.z + o->aboveeye - o->radius, middle.z);
        vec closest(o->o.x, o->o.y, std::clamp(v.z, low, high));
        return max(closest.dist(v) - o->radius, 0.0f);
    }

    void radialeffect(dynent *o, const vec &v, const vec &vel, int qdam, gameent *at, int atk)
    {
        if(o->state!=ClientState_Alive)
        {
            return;
        }
        vec dir;
        float dist = projdist(o, dir, v, vel);
        if(dist<attacks[atk].exprad)
        {
            float damage = qdam*(1-dist/EXP_DISTSCALE/attacks[atk].exprad);
            if(o==at)
            {
                damage /= EXP_SELFDAMDIV;
            }
            if(damage > 0)
            {
                hit(max(static_cast<int>(damage), 1), o, at, dir, atk, dist);
            }
        }
    }

    void explode(bool local, gameent *owner, const vec &v, const vec &vel, dynent *safe, int damage, int atk)
    {
        particle_splash(Part_Spark, 200, 300, v, 0x50CFE5, 0.45f);
        soundmain.playsound(Sound_PulseExplode, &v);
        particle_fireball(v, 1.15f*attacks[atk].exprad, Part_PulseBurst, static_cast<int>(attacks[atk].exprad*20), 0x50CFE5, 4.0f);
        vec debrisorigin = vec(v).sub(vec(vel).mul(5));
        adddynlight(safe ? v : debrisorigin, 2*attacks[atk].exprad, vec(1.0f, 3.0f, 4.0f), 350, 40, 0, attacks[atk].exprad, vec(0.5f, 1.5f, 2.0f));

        if(!local)
        {
            return;
        }
        int numdyn = numdynents;
        for(int i = 0; i < numdyn; ++i)
        {
            dynent *o = iterdynents(i);
            if(o->o.reject(v, o->radius + attacks[atk].exprad) || o==safe)
            {
                continue;
            }
            radialeffect(o, v, vel, damage, owner, atk);
        }
    }

    void pulsestain(const projectile &p, const vec &pos)
    {
        vec dir = vec(p.dir).neg();
        float rad = attacks[p.atk].exprad*0.75f;
        addstain(Stain_PulseGlow, pos, dir, rad, 0x50CFE5);
    }

    void projsplash(projectile &p, const vec &v, dynent *safe)
    {
        explode(p.local, p.owner, v, p.dir, safe, attacks[p.atk].damage, p.atk);
        pulsestain(p, v);
    }

    void explodeeffects(int atk, const gameent *d, bool local, int id)
    {
        if(local)
        {
            return;
        }
        switch(atk)
        {
            case Attack_PulseShoot: //pulse rifle is currently the only weapon to do this
            {
                for(uint i = 0; i < projs.size(); i++)
                {
                    projectile &p = projs[i];
                    if(p.atk == atk && p.owner == d && p.id == id && !p.local)
                    {
                        vec pos = vec(p.offset).mul(p.offsetmillis/static_cast<float>(offsetmillis)).add(p.o);
                        explode(p.local, p.owner, pos, p.dir, nullptr, 0, atk);
                        pulsestain(p, pos);
                        projs.erase(projs.begin() + i);
                        break;
                    }
                }
                break;
            }
            default:
            {
                break;
            }
        }
    }
    /*projdamage: checks if projectile damages a particular dynent
     * Arguments:
     *  o: dynent (player ent) to check damage for
     *  p: projectile object to attempt to damage with
     *  v: the displacement vector that the projectile is currently stepping over
     * Returns:
     *  (bool) true if projectile damages dynent, false otherwise
     */
    bool projdamage(dynent *o, projectile &p, const vec &v)
    {
        if(o->state!=ClientState_Alive) //do not beat dead horses (or clients)
        {
            return false;
        }
        if(!intersect(o, p.o, v, attacks[p.atk].margin)) //do not damange unless collided
        {
            return false;
        }
        projsplash(p, v, o); //check splash
        vec dir;
        projdist(o, dir, v, p.dir);
        hit(attacks[p.atk].damage, o, p.owner, dir, p.atk, 0);
        return true;
    }

    /*explodecubes: deletes some cubes at a world vector location
     * Arguments:
     *  loc: world vector to destroy
     *  gridpower: size of cube to blow up
     * Returns:
     *  void
     */
    void explodecubes(ivec loc, int gridpower, int bias = 1)
    {
        //define selection boundaries that align with gridpower
        ivec minloc( loc.x - loc.x % gridpower -2*gridpower,
                     loc.y - loc.y % gridpower -2*gridpower,
                     loc.z - loc.z % gridpower -(2-bias)*gridpower);
        ivec maxlocz(3,3,5);
        ivec maxlocy(3,5,3);
        ivec maxlocx(5,3,3);
        selinfo sel;
        sel.grid = gridpower;
        sel.o = minloc + ivec(gridpower,gridpower,0);
        sel.s = maxlocz;
        mpdelcube(sel, true);
        sel.o = minloc + ivec(gridpower,0,gridpower);
        sel.s = maxlocy;
        mpdelcube(sel, true);
        sel.o = minloc + ivec(0,gridpower,gridpower);
        sel.s = maxlocx;
        mpdelcube(sel, true);
    }

    /*placecube: places a cube volume at a world vector location
     * Arguments:
     *  loc: world vector to fill
     *  gridpower: size of cubes to place
     *  tex: index of the texture to place
     *  offset: offset for origin of cube volume
     *  size: size in cubes for cube volume (goes +x,+y,+z from origin)
     * Returns:
     *  void
     */
    void placecube(ivec loc, int gridpower, int tex, ivec offset = ivec(0,0,0), ivec size = ivec(1,1,1))
    {
        int gridpow = static_cast<int>(pow(2,gridpower));
        ivec minloc( loc.x - loc.x % gridpow,
                     loc.y - loc.y % gridpow,
                     loc.z - loc.z % gridpow );
        selinfo sel;
        sel.o = minloc + offset;
        sel.s = size;
        mpplacecube(sel, tex, true);
    }

    vec calctrajectory(projectile &p, int iter)
    {
        float timefactor = elapsedtime/16.7f; //normalize physics to 60 fps
        vec v = vec(p.o);
        for(int i = 0; i < iter; ++i)
        {
            v.add(p.dir*p.speed*timefactor).sub(vec(0, 0, timefactor*0.001*(lastmillis-p.spawntime)*p.gravity)); //set v as current particle location o plus dv
        }
        return v;
    }

    void updateprojectiles(int time)
    {
        float timefactor = elapsedtime/16.7f; //normalize physics to 60 fps
        if(projs.empty())
        {
            return;
        }
        gameent *noside = hudplayer();
        for(uint i = 0; i < projs.size(); i++) //loop through all projectiles in the game
        {
            projectile &p = projs[i];
            p.offsetmillis = max(p.offsetmillis-time, 0);
            vec dv = p.dir; //displacement vector
            dv.mul(p.speed*timefactor);
            vec v = calctrajectory(p, 1);
            bool exploded = false;
            hits.clear();
            if(p.local) //if projectile belongs to a local client
            {
                vec halfdv = vec(dv).mul(0.5f),
                    bo     = vec(p.o).add(halfdv); //half the displacement vector halfdv; set bo like v except with halfdv
                float br = max(fabs(halfdv.x), fabs(halfdv.y)) + 1 + attacks[p.atk].margin;
                if(!attacks[p.atk].water)
                {
                    cube projcube = rootworld.lookupcube(static_cast<ivec>(p.o)); //cube located at projectile loc
                    if(getmaterial(projcube) == Mat_Water)
                    {
                        projsplash(p, v, nullptr);
                        exploded = true;
                    }//projs that can't go through water
                }
                for(int j = 0; j < numdynents; ++j)
                {
                    dynent *o = iterdynents(j); //start by setting cur to current dynent in loop
                    //check if dynent in question is the owner of the projectile or is within the bounds of some other dynent (actor)
                    //if projectile is owned by a player or projectile is not within the bounds of a dynent, skip explode check
                    if(p.owner==o || o->o.reject(bo, o->radius + br))
                    {
                        continue;
                    }
                    if(projdamage(o, p, v))
                    {
                        exploded = true;
                        break;
                    } //damage check
                }
            }
            if(!exploded) //if we haven't already hit somebody, start checking for collisions with cube geometry
            {
                vec loc = p.dir;
                if(raycubepos(p.o, p.dir, loc.mul(p.speed*(p.deathtime-curtime)), 0, Ray_ClipMat|Ray_AlphaPoly) <= 4)
                {
                    projsplash(p, v, nullptr);
                    exploded = true;
                }
                else
                {
                    vec pos = vec(p.offset).mul(p.offsetmillis/static_cast<float>(offsetmillis)).add(v);
                    particle_splash(Part_PulseFront, 1, 1, pos, 0x50CFE5, 2.4f, 150, 20);
                    if(p.owner != noside) //noside is the hud player, so if the projectile is somebody else's
                    {
                        float len = min(20.0f, vec(p.offset).add(p.from).dist(pos)); //projectiles are at least 20u long
                        vec dir = vec(dv).normalize(),
                            tail = vec(dir).mul(-len).add(pos), //tail extends >=20u behind projectile point
                            head = vec(dir).mul(2.4f).add(pos); // head extends 2.4u ahead
                        particle_flare(tail, head, 1, Part_PulseSide, 0x50CFE5, 2.5f);
                    }
                }
            }
            if(exploded)
            {
                switch(attacks[p.atk].worldfx)
                {
                    case 1:
                    {
                        explodecubes(static_cast<ivec>(p.o), 4);
                        break;
                    }
                }
                if(p.local)
                {
                    addmsg(NetMsg_Explode, "rci3iv", p.owner, lastmillis-maptime, p.atk, p.id-maptime,
                            hits.size(), hits.size()*sizeof(hitmsg)/sizeof(int), hits.data()); //sizeof int should always be 4 bytes
                }
                projs.erase(projs.begin() + i);
                i--;
            }
            else
            {
                p.o = v; //if no collision stuff happened set the new position
            }
        }
    }

    /*railhit: creates a hitscan beam between points
     * Arguments:
     *  from: the origin location
     *  to: the destination location
     *  stain: whether to stain the hit point
     * Returns:
     *  void
     */
    void railhit(const vec &from, const vec &to, bool stain = true)
    {
        vec dir = vec(from).sub(to).safenormalize();
        if(stain)
        {
            addstain(Stain_RailHole, to, dir, 2.0f);
            addstain(Stain_RailGlow, to, dir, 2.5f, 0x50CFE5);
        }
        adddynlight(vec(to).madd(dir, 4), 10, vec(0.25f, 0.75f, 1.0f), 225, 75);
    }

    void shoteffects(int atk, const vec &from, const vec &to, gameent *d, bool local, int id, int prevaction)     // create visual effect from a shot
    {
        switch(atk)
        {
            case Attack_PulseShoot:
            {
                if(d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 140, Part_PulseMuzzleFlash, 0x50CFE5, 3.50f, d); //place a light that runs with the shot projectile
                }
                newprojectile(from, to, attacks[atk].projspeed, local, id, d, atk, attacks[atk].time, attacks[atk].gravity);
                break;
            }
            case Attack_EngShoot:
            {
                if(!local)
                {
                    railhit(from, to, false);
                }
                break;
            }
            case Attack_RailShot:
            {
                particle_splash(Part_Spark, 200, 250, to, 0x50CFE5, 0.45f);
                particle_flare(hudgunorigin(from, to, d), to, 500, Part_RailTrail, 0x50CFE5, 0.5f);
                if(d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 140, Part_RailMuzzleFlash, 0x50CFE5, 2.75f, d);
                }
                adddynlight(hudgunorigin(d->o, to, d), 35, vec(0.25f, 0.75f, 1.0f), 75, 75, DynLight_Flash, 0, vec(0, 0, 0), d); //place a light for the muzzle flash
                if(!local)
                {
                    railhit(from, to);
                }
                break;
            }
            case Attack_CarbineShoot:
            {
                particle_splash(Part_Spark, 200, 250, to, 0x50CFE5, 0.45f);
                particle_flare(hudgunorigin(from, to, d), to, 500, Part_RailTrail, 0x50CFE5, 0.5f);
                if(d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 140, Part_RailMuzzleFlash, 0x50CFE5, 2.75f, d);
                }
                adddynlight(hudgunorigin(d->o, to, d), 35, vec(0.25f, 0.75f, 1.0f), 75, 75, DynLight_Flash, 0, vec(0, 0, 0), d); //place a light for the muzzle flash
                if(!local)
                {
                    railhit(from, to);
                }
                break;
            }
            case Attack_ShotgunShoot:
            {
                particle_splash(Part_Spark, 200, 250, to, 0xFFCFE5, 0.45f);
                for(int i = 0; i < attacks[atk].rays; ++i)
                {
                    particle_flare(hudgunorigin(from, to, d), rays[i], 500, Part_RailTrail, 0x50CFE5, 0.5f);
                }
                if(d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 140, Part_RailMuzzleFlash, 0x50CFE5, 2.75f, d);
                }
                adddynlight(hudgunorigin(d->o, to, d), 35, vec(0.25f, 0.75f, 1.0f), 75, 75, DynLight_Flash, 0, vec(0, 0, 0), d); //place a light for the muzzle flash
                if(!local)
                {
                    railhit(from, to);
                }
                break;
            }
            default:
            {
                break;
            }
        }

        if(d==hudplayer())
        {
            soundmain.playsound(attacks[atk].hudsound, nullptr);
        }
        else
        {
            soundmain.playsound(attacks[atk].sound, &d->o);
        }
    }

    float intersectdist = 1e16f;

    bool intersect(dynent *d, const vec &from, const vec &to, float margin, float &dist)   // if lineseg hits entity bounding box
    {
        vec bottom(d->o), top(d->o);
        bottom.z -= d->eyeheight + margin;
        top.z += d->aboveeye + margin;
        return linecylinderintersect(from, to, bottom, top, d->radius + margin, dist);
    }

    dynent *intersectclosest(const vec &from, const vec &to, gameent *at, float margin, float &bestdist)
    {
        dynent *best = nullptr;
        bestdist = 1e16f;
        for(int i = 0; i < numdynents; ++i)
        {
            dynent *o = iterdynents(i);
            if(o != nullptr)
            {
                if(o==at || o->state!=ClientState_Alive)
                {
                    continue;
                }
                float dist;
                if(!intersect(o, from, to, margin, dist))
                {
                    continue;
                }
                if(dist<bestdist)
                {
                    best = o;
                    bestdist = dist;
                }
            }
        }
        return best;
    }

    void shorten(const vec &from, vec &target, float dist)
    {
        target.sub(from).mul(min(1.0f, dist)).add(from);
    }

    void raydamage(vec &from, vec &to, gameent *d, int atk)
    {
        dynent *o;
        float dist;
        int margin = attacks[atk].margin;
        if((o = intersectclosest(from, to, d, margin, dist)))
        {
            shorten(from, to, dist);
            railhit(from, to, false);
            hitpush(attacks[atk].damage, o, d, from, to, atk, 1);
        }
        else
        {
            railhit(from, to);
        }
    }

    void shoot(gameent *d, const vec &targ)
    {
        int prevaction = d->lastaction,
            attacktime = lastmillis-prevaction;
        if(attacktime<d->gunwait)
        {
            return;
        }
        d->gunwait = 0;
        if(!d->attacking)
        {
            return;
        }
        int gun = d->gunselect,
            act = d->attacking,
            atk = guns[gun].attacks[act];
        if(d->heat[gun] > attacks[atk].maxheat) // check if weapon has overheated
        {
            return;
        }
        d->lastaction = lastmillis;
        d->lastattack = atk;
        if(!d->ammo[gun])
        {
            if(d==player1)
            {
                d->gunwait = 600;
                d->lastattack = -1;
                weaponswitch(d);
            }
            return;
        }
        d->ammo[gun] -= attacks[atk].use;

        vec from = d->o,
            to = targ,
            dir = vec(to).sub(from).safenormalize();
        float dist = to.dist(from);
        if(!(d->physstate >= PhysEntState_Slope && d->crouching && d->crouched()))
        {
            vec kickback = vec(dir).mul(attacks[atk].kickamount*-2.5f);
            if(lastmillis - d->parachutetime < parachutemaxtime)
            {
                kickback.mul(parachutekickfactor);
            }
            d->vel.add(kickback);
        }
        int projdist = attacks[atk].projspeed ? attacks[atk].time*attacks[atk].projspeed : attacks[atk].time;
        float shorten = attacks[atk].time && dist > projdist ? projdist : 0,
              barrier = rootworld.raycube(d->o, dir, dist, Ray_ClipMat|Ray_AlphaPoly);
        if(barrier > 0 && barrier < dist && (!shorten || barrier < shorten))
        {
            shorten = barrier;
        }
        if(shorten)
        {
            to = vec(dir).mul(shorten).add(from).add(dir.mul(projdepth));
        }

        if(attacks[atk].rays > 1)
        {
            createrays(atk, from, to);
        }
        else if(attacks[atk].spread)
        {
            offsetray(from, to, attacks[atk].spread, attacks[atk].time, to);
        }
        hits.clear();

        int blocktex = d->team + 1; //2 and 3 are the indices for team blocks
        if(!attacks[atk].projspeed)
        {
            if(attacks[atk].worldfx == 2)
            {
                if(rootworld.lookupcube(static_cast<ivec>(to)).issolid())
                {
                    //note: 8 and 3 are linked magic numbers (gridpower)
                    ivec offsetloc = static_cast<ivec>(to) + ivec(0,0,8);
                    placecube(offsetloc, 2, blocktex);
                }
                else if(checkcubefill(rootworld.lookupcube(static_cast<ivec>(to))))
                {
                    ivec offsetloc = static_cast<ivec>(to) + ivec(0,0,8);
                    placecube(offsetloc, 2, blocktex);
                }
                else if (!(rootworld.lookupcube(static_cast<ivec>(to)).isempty()))
                {
                    placecube(static_cast<ivec>(to), 3, blocktex);
                }
            }
            else
            {
                if(attacks[atk].rays > 1)
                {
                    for(int i = 0; i < attacks[atk].rays; ++i)
                    {
                        raydamage(from, rays[i], d, atk);
                    }
                }
                else
                {
                    raydamage(from, to, d, atk);
                }
            }
        }
        shoteffects(atk, from, to, d, true, 0, prevaction);

        if(d==player1 || d->ai)
        {
            if(d->sprinting == -1)
            {
                d->sprinting = 1;
            }
            addmsg(NetMsg_Shoot, "rci2i6iv", d, lastmillis-maptime, atk,
                   static_cast<int>(from.x*DMF), static_cast<int>(from.y*DMF), static_cast<int>(from.z*DMF),
                   static_cast<int>(to.x*DMF),   static_cast<int>(to.y*DMF),   static_cast<int>(to.z*DMF),
                   hits.size(), hits.size()*sizeof(hitmsg)/sizeof(int), hits.data()); //sizeof int should always equal 4 (bytes) = 32b
        }

        d->gunwait = attacks[atk].attackdelay;
        if(d->ai)
        {
            d->gunwait += static_cast<int>(d->gunwait*(((101-d->skill)+randomint(111-d->skill))/100.f));
        }
        d->heat[gun] += attacks[atk].heat;
        d->totalshots += attacks[atk].damage*attacks[atk].rays;
    }

    void adddynlights()
    {
        for(uint i = 0; i < projs.size(); i++)
        {
            projectile &p = projs[i];
            if(p.atk!=Attack_PulseShoot)
            {
                continue;
            }
            vec pos(p.o);
            pos.add(vec(p.offset).mul(p.offsetmillis/static_cast<float>(offsetmillis)));
            adddynlight(pos, 20, vec(0.25f, 0.75f, 1.0f));
        }
    }

    void renderbouncers()
    {
        float yaw, pitch;
        for(uint i = 0; i < bouncers.size(); i++)
        {
            bouncer &bnc = bouncers[i];
            vec pos(bnc.o);
            pos.add(vec(bnc.offset).mul(bnc.offsetmillis/static_cast<float>(offsetmillis)));
            vec vel(bnc.vel);
            if(vel.magnitude() <= 25.0f)
            {
                yaw = bnc.lastyaw;
            }
            else
            {
                vectoryawpitch(vel, yaw, pitch);
                yaw += 90;
                bnc.lastyaw = yaw;
            }
            pitch = -bnc.roll;
            const char *mdl = nullptr;
            int cull = Model_CullVFC|Model_CullDist|Model_CullOccluded;
            float fade = 1;
            if(bnc.lifetime < 250)
            {
                fade = bnc.lifetime/250.0f;
            }
            switch(bnc.bouncetype)
            {
                default:
                {
                    continue;
                }
            }
            rendermodel(mdl, +Anim_Mapmodel | +Anim_Loop, pos, yaw, pitch, 0, cull, nullptr, nullptr, 0, 0, fade);
        }
    }
    void removeweapons(const gameent *d)
    {
        removebouncers(d);
        removeprojectiles(d);
    }

    void updateheat()
    {
        static int lasttime = 0;
        int heatticktime = 50;
        if(lastmillis - heatticktime >= lasttime)
        {
            lasttime = lastmillis;
            for(int i = 0; i<Gun_NumGuns; i++)
            {
                for(uint j = 0; j < players.size(); j++)
                {
                    if(players[j]->ai)
                    {
                        players[j]->heat[i] -= 5;
                    }
                }
                if(player1->heat[i] > 0)
                {
                    player1->heat[i] -= 5;
                }
            }
        }
    }

    void updateweapons(int curtime)
    {
        updateprojectiles(curtime);
        if(player1->clientnum>=0 && player1->state==ClientState_Alive)
        {
            shoot(player1, worldpos); // only shoot when connected to server
        }
        updateheat();
        updatebouncers(curtime); // need to do this after the player shoots so bouncers don't end up inside player's BB next frame
    }

    void avoidweapons(ai::avoidset &obstacles, float radius)
    {
        for(uint i = 0; i < projs.size(); i++)
        {
            projectile &p = projs[i];
            obstacles.avoidnear(nullptr, p.o.z + attacks[p.atk].exprad + 1, p.o, radius + attacks[p.atk].exprad);
        }
    }
};

