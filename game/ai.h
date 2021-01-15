struct gameent;

const int maxbots = 32;

enum
{
    AI_None = 0,
    AI_Bot,
    AI_Max
};

namespace ai
{
    const int maxwaypoints = USHRT_MAX - 2;
    const int maxwaypointlinks = 6;
    const int waypointradius = 16;

    const float minwpdist       = 4.f;     // is on top of
    const float closedist       = 32.f;    // is close
    const float fardist         = 128.f;   // too far to remap close
    const float jumpmin         = 4.f;     // decides to jump
    const float jumpmax         = 32.f;    // max jump
    const float sightmin        = 64.f;    // minimum line of sight
    const float sightmax        = 1024.f;  // maximum line of sight
    const float viewmin         = 90.f;    // minimum field of view
    const float viewmax         = 180.f;   // maximum field of view

    class waypoint
    {
        public:
            vec o;
            int weight;
            ushort links[maxwaypointlinks];
            ushort route, prev;
            float curscore, estscore;

            waypoint()
            {
            }

            waypoint(const vec &o, int weight = 0) : o(o), weight(weight), route(0)
            {
                memset(links, 0, sizeof(links));
            }

            int score() const
            {
                return static_cast<int>(curscore) + static_cast<int>(estscore);
            }

            int find(int wp)
            {
                for(int i = 0; i < maxwaypointlinks; ++i)
                {
                    if(links[i] == wp)
                    {
                        return i;
                    }
                }
                return -1;
            }

        private:
            bool haslinks()
            {
                return links[0]!=0;
            }
    };
    extern vector<waypoint> waypoints;

    inline bool iswaypoint(int n)
    {
        return n > 0 && n < waypoints.length();
    }

    extern int showwaypoints, dropwaypoints;
    extern int closestwaypoint(const vec &pos, float mindist, bool links, gameent *d = NULL);
    extern void findwaypointswithin(const vec &pos, float mindist, float maxdist, vector<int> &results);
    extern void inferwaypoints(gameent *d, const vec &o, const vec &v, float mindist = ai::closedist);

    class avoidset
    {
        public:
            struct obstacle
            {
                void *owner;
                int numwaypoints;
                float above;

                obstacle(void *owner, float above = -1) : owner(owner), numwaypoints(0), above(above) {}
            };
            vector<obstacle> obstacles;
            vector<int> waypoints;

            void clear()
            {
                obstacles.setsize(0);
                waypoints.setsize(0);
            }

            void add(avoidset &avoid)
            {
                waypoints.put(avoid.waypoints.getbuf(), avoid.waypoints.length());
                for(int i = 0; i < avoid.obstacles.length(); i++)
                {
                    obstacle &o = avoid.obstacles[i];
                    if(obstacles.empty() || o.owner != obstacles.last().owner)
                    {
                        add(o.owner, o.above);
                    }
                    obstacles.last().numwaypoints += o.numwaypoints;
                }
            }

            #define LOOP_AVOID(v, d, body) \
                if(!(v).obstacles.empty()) \
                { \
                    int cur = 0; \
                    for(int i = 0; i < (v).obstacles.length(); i++) \
                    { \
                        const ai::avoidset::obstacle &ob = (v).obstacles[i]; \
                        int next = cur + ob.numwaypoints; \
                        if(ob.owner != d) \
                        { \
                            for(; cur < next; cur++) \
                            { \
                                int wp = (v).waypoints[cur]; \
                                body; \
                            } \
                        } \
                        cur = next; \
                    } \
                }

            bool find(int n, gameent *d) const
            {
                LOOP_AVOID(*this, d,
                {
                    if(wp == n)
                    {
                        return true;
                    }
                });
                return false;
            }

            void avoidnear(void *owner, float above, const vec &pos, float limit);
            int remap(gameent *d, int n, vec &pos, bool retry = false);

        private:
            void add(void *owner, float above)
            {
                obstacles.add(obstacle(owner, above));
            }

            void add(void *owner, float above, int wp)
            {
                if(obstacles.empty() || owner != obstacles.last().owner)
                {
                    add(owner, above);
                }
                obstacles.last().numwaypoints++;
                waypoints.add(wp);
            }
    };

    extern bool route(gameent *d, int node, int goal, vector<int> &route, const avoidset &obstacles, int retries = 0);
    extern void navigate();
    extern void clearwaypoints(bool full = false);
    extern void seedwaypoints();
    extern void loadwaypoints(bool force = false, const char *mname = NULL);
    extern void savewaypoints(bool force = false, const char *mname = NULL);

    // ai state information for the owner client
    enum
    {
        AIState_Wait = 0,      // waiting for next command
        AIState_Defend,        // defend goal target
        AIState_Pursue,        // pursue goal target
        AIState_Interest,      // interest in goal entity
        AIState_Max,
    };

    enum
    { //renamed to Travel, but "T" could mean something else
        AITravel_Node,
        AITravel_Player,
        AITravel_Affinity,
        AITravel_Entity,
        AITravel_Max,
    };

    struct interest
    {
        int state, node, target, targtype;
        float score;
        interest() : state(-1), node(-1), target(-1), targtype(-1), score(0.f) {}
        ~interest() {}
    };

    struct aistate
    {
        int type, millis, targtype, target, idle;
        bool override;

        aistate(int m, int t, int r = -1, int v = -1) : type(t), millis(m), targtype(r), target(v)
        {
            reset();
        }
        ~aistate() {}

        void reset()
        {
            idle = 0;
            override = false;
        }
    };

    const int numprevnodes = 6;
    extern vec aitarget;

    class aiinfo
    {
        public:
            int enemy, weappref, targnode, lastcheck;
            int prevnodes[numprevnodes];
            vector<aistate> state;
            vector<int> route;
            vec spot;
            aiinfo()
            {
                clearsetup();
                reset();
                for(int k = 0; k < 3; ++k)
                {
                    views[k] = 0.f;
                }
            }
            ~aiinfo() {}

            bool hasprevnode(int n) const
            {
                for(int i = 0; i < numprevnodes; ++i)
                {
                    if(prevnodes[i] == n)
                    {
                        return true;
                    }
                }
                return false;
            }

            void addprevnode(int n)
            {
                if(prevnodes[0] != n)
                {
                    memmove(&prevnodes[1], prevnodes, sizeof(prevnodes) - sizeof(prevnodes[0]));
                    prevnodes[0] = n;
                }
            }

            void init(gameent *d, int at, int ocn, int sk, int bn, int pm, int col, const char *name, int team);
            void spawned(gameent *d);
            void damaged(gameent *d, gameent *e);
            void killed(gameent *d, gameent *e);
            void think(gameent *d, bool run);

        private:
            int enemyseen, enemymillis,
                targlast, targtime, targseq,
                lastrun, lasthunt, lastaction,
                jumpseed, jumprand,
                blocktime,
                huntseq,
                blockseq,
                lastaimrnd;
            float targyaw,
                  targpitch,
                  views[3],
                  aimrnd[3];
            bool dontmove,
                 becareful,
                 tryreset,
                 trywipe;
            vec target;
            gameent *parent;

            void clearsetup()
            {
                weappref = Gun_Rail;
                spot = target = vec(0, 0, 0);
                lastaction = lasthunt = lastcheck = enemyseen = enemymillis = blocktime = huntseq = blockseq = targtime = targseq = lastaimrnd = 0;
                lastrun = jumpseed = lastmillis;
                jumprand = lastmillis+5000;
                targnode = targlast = enemy = -1;
            }

            void clear(bool prev = false)
            {
                if(prev)
                {
                    memset(prevnodes, -1, sizeof(prevnodes));
                }
                route.setsize(0);
            }

            void wipe(bool prev = false)
            {
                clear(prev);
                state.setsize(0);
                addstate(AIState_Wait);
                trywipe = false;
            }

            void clean(bool tryit = false)
            {
                if(!tryit)
                {
                    becareful = dontmove = false;
                }
                targyaw = randomint(360);
                targpitch = 0.f;
                tryreset = tryit;
            }

            void reset(bool tryit = false)
            {
                wipe();
                clean(tryit);
            }

            aistate &addstate(int t, int r = -1, int v = -1)
            {
                return state.add(aistate(lastmillis, t, r, v));
            }

            void removestate(int index = -1)
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

            aistate &getstate(int idx = -1)
            {
                if(state.inrange(idx))
                {
                    return state[idx];
                }
                return state.last();
            }

            aistate &switchstate(aistate &b, int t, int r = -1, int v = -1)
            {
                if((b.type == t && b.targtype == r) || (b.type == AIState_Interest && b.targtype == AITravel_Node))
                {
                    b.millis = lastmillis;
                    b.target = v;
                    b.reset();
                    return b;
                }
                return addstate(t, r, v);
            }
            float viewdist(int skill);
            float viewfieldx(int skill);
            float viewfieldy(int skill);
            bool canmove(gameent *d);
            float attackmindist(int atk);
            float attackmaxdist(int atk);
            bool attackrange(gameent *d, int atk, float dist);
            bool targetable(gameent *d, gameent *e);
            bool getsight(vec &o, float yaw, float pitch, vec &q, vec &v, float mdist, float fovx, float fovy);
            bool cansee(gameent *d, vec &x, vec &y, vec &targ = aitarget);
            bool canshoot(gameent *d, int atk, gameent *e);
            bool canshoot(gameent *d, int atk);
            bool hastarget(gameent *d, int atk, aistate &b, gameent *e, float yaw, float pitch, float dist);
            vec getaimpos(gameent *d, int atk, gameent *e);
            void create(gameent *d);
            void destroy(gameent *d);
            bool checkothers(vector<int> &targets, gameent *d = NULL, int state = -1, int targtype = -1, int target = -1, bool teams = false, int *members = NULL);
            bool randomnode(gameent *d, aistate &b, const vec &pos, float guard, float wander);
            bool randomnode(gameent *d, aistate &b, float guard, float wander);
            bool badhealth(gameent *d);
            bool isenemy(gameent *d, aistate &b, const vec &pos, float guard = sightmin, int pursue = 0);
            bool patrol(gameent *d, aistate &b, const vec &pos, float guard = sightmin, float wander = sightmax, int walk = 1, bool retry = false);
            bool defend(gameent *d, aistate &b, const vec &pos, float guard = sightmin, float wander = sightmax, int walk = 1);
            bool violence(gameent *d, aistate &b, gameent *e, int pursue = 0);
            bool istarget(gameent *d, aistate &b, int pursue = 0, bool force = false, float mindist = 0.f);
            int isgoodammo(int gun);
            bool hasgoodammo(gameent *d);
            void assist(gameent *d, aistate &b, vector<interest> &interests, bool all = false, bool force = false);
            bool parseinterests(gameent *d, aistate &b, vector<interest> &interests, bool override = false, bool ignore = false);
            bool find(gameent *d, aistate &b, bool override = false);
            bool findassist(gameent *d, aistate &b, bool override = false);
            void findorientation(vec &o, float yaw, float pitch, vec &pos);
            void setup(gameent *d);
            bool check(gameent *d, aistate &b);
            int dowait(gameent *d, aistate &b);
            int dodefend(gameent *d, aistate &b);
            int dointerest(gameent *d, aistate &b);
            int dopursue(gameent *d, aistate &b);
            int closenode(gameent *d);
            int wpspot(gameent *d, int n, bool check = false);
            int randomlink(gameent *d, int n);
            bool anynode(gameent *d, aistate &b, int len = numprevnodes);
            bool hunt(gameent *d, aistate &b);
            void jumpto(gameent *d, aistate &b, const vec &pos);
            void fixfullrange(float &yaw, float &pitch, float &roll, bool full);
            void fixrange(float &yaw, float &pitch);
            void getyawpitch(const vec &from, const vec &pos, float &yaw, float &pitch);
            void scaleyawpitch(float &yaw, float &pitch, float targyaw, float targpitch, float frame, float scale);
            int process(gameent *d, aistate &b);
            bool hasrange(gameent *d, gameent *e, int weap);
            bool request(gameent *d, aistate &b);
            void timeouts(gameent *d, aistate &b);
            void logic(gameent *d, aistate &b, bool run);
    };

    extern avoidset obstacles;

    extern void avoid();
    extern void update();
    extern float viewdist(int x = 101);
    extern float viewfieldx(int x = 101);
    extern float viewfieldy(int x = 101);
    extern bool targetable(gameent *d, gameent *e);

    extern void init(gameent *d, int at, int on, int sk, int bn, int pm, int col, const char *name, int team);
    extern void update();
    extern void avoid();
    extern void think(gameent *d, bool run);

    bool checkroute(gameent *d, int n);
    extern bool badhealth(gameent *d);
    extern bool makeroute(gameent *d, aistate &b, int node, bool changed = true, int retries = 0);
    extern bool makeroute(gameent *d, aistate &b, const vec &pos, bool changed = true, int retries = 0);
    extern bool randomnode(gameent *d, aistate &b, const vec &pos, float guard = sightmin, float wander = sightmax);
    extern bool randomnode(gameent *d, aistate &b, float guard = sightmin, float wander = sightmax);
    extern bool violence(gameent *d, aistate &b, gameent *e, int pursue = 0);
    extern void spawned(gameent *d);
    extern void damaged(gameent *d, gameent *e);
    extern void killed(gameent *d, gameent *e);

    extern void render();
}


