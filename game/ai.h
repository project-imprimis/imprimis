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

    extern int aidebug;

    class waypoint final
    {
        public:
            vec o;
            int weight;
            ushort links[maxwaypointlinks];
            ushort route, prev;
            float curscore, estscore;

            waypoint() : o(0,0,0), weight(), links(), route(), prev(), curscore(), estscore() {}

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

            bool haslinks()
            {
                return links[0]!=0;
            }
    };
    extern std::vector<waypoint> waypoints;

    inline bool iswaypoint(uint n)
    {
        return n > 0 && n < waypoints.size();
    }

    extern int showwaypoints, dropwaypoints;
    extern int closestwaypoint(const vec &pos, float mindist, bool links, gameent *d = NULL);
    extern void findwaypointswithin(const vec &pos, float mindist, float maxdist, std::vector<int> &results);
    extern void inferwaypoints(gameent *d, const vec &o, const vec &v, float mindist = ai::closedist);

    class avoidset final
    {
        public:
            struct obstacle
            {
                void *owner;
                int numwaypoints;
                float above;

                obstacle(void *owner, float above = -1) : owner(owner), numwaypoints(0), above(above) {}
            };
            std::vector<obstacle> obstacles;
            std::vector<int> waypoints;

            void clear()
            {
                obstacles.clear();
                waypoints.clear();
            }

            void add(avoidset &avoid)
            {
                waypoints.insert(waypoints.begin(), avoid.waypoints.begin(), avoid.waypoints.end());
                for(obstacle& o : avoid.obstacles)
                {
                    if(obstacles.empty() || o.owner != obstacles.back().owner)
                    {
                        add(o.owner, o.above);
                    }
                    obstacles.back().numwaypoints += o.numwaypoints;
                }
            }

            #define LOOP_AVOID(v, d, body) \
                if(!(v).obstacles.empty()) \
                { \
                    int cur = 0; \
                    for(uint i = 0; i < (v).obstacles.size(); i++) \
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
                obstacles.push_back(obstacle(owner, above));
            }

            void add(void *owner, float above, int wp)
            {
                if(obstacles.empty() || owner != obstacles.back().owner)
                {
                    add(owner, above);
                }
                obstacles.back().numwaypoints++;
                waypoints.push_back(wp);
            }
    };

    extern bool wproute(gameent *d, int node, int goal, std::vector<int> &route, const avoidset &obstacles, int retries = 0);
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
        AIState_Max,
    };

    enum
    { //renamed to Travel, but "T" could mean something else
        AITravel_Node,
        AITravel_Player,
        AITravel_Entity,
        AITravel_Max,
    };

    struct interest final
    {
        int state, node, target, targtype;
        float score;
        interest() : state(-1), node(-1), target(-1), targtype(-1), score(0.f) {}
        ~interest() {}
    };

    struct aistate final
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

    //the ai base class-- the interface by which different AIs can connect to the game
    //by itself, can do nothing; must be extended by derived classes
    class aiinfo
    {
        public:
            static constexpr int statereservedsize = 100; //pre-allocate to avoid pointer invalidation
            int enemy, weappref, targnode, lastcheck;
            int prevnodes[numprevnodes];
            std::list<aistate> state;
            vec spot;
            gameent * aiplayer;
            aiinfo()
            {
            };
            virtual ~aiinfo() {};

            virtual bool hasprevnode(int n) const = 0;
            virtual void addprevnode(int n) = 0;
            virtual bool init(gameent *d, int at, int ocn, int sk, int bn, int pm, int col, const char *name, int team) = 0;
            virtual void spawned(gameent *d) = 0;
            virtual void damaged(const gameent *e) = 0;
            virtual void killed() = 0;
            virtual void think(gameent *d, bool run) = 0;
    };

    //specific ai type that uses waypoints--
    class waypointai final : public aiinfo
    {
        public:
            std::vector<int> route;
            waypointai()
            {
                clearsetup();
                reset();
                for(int k = 0; k < 3; ++k)
                {
                    views[k] = 0.f;
                }
            }
            ~waypointai() {}

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

            bool init(gameent *d, int at, int ocn, int sk, int bn, int pm, int col, const char *name, int team);
            void spawned(gameent *d);
            void damaged(const gameent *e);
            void killed();
            void think(gameent *d, bool run);
            void drawroute(float amt);

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
            std::vector<int> remapping;

            void clearsetup();
            void clear(bool prev = false);
            void wipe(bool prev = false);
            void clean(bool tryit = false);
            void reset(bool tryit = false);
            aistate &addstate(int t, int r = -1, int v = -1);
            aistate &getstate();
            aistate &switchstate(aistate &b, int t, int r = -1, int v = -1);

            float viewdist(int skill) const;
            float viewfieldx(int skill) const;
            float viewfieldy(int skill) const;
            bool canmove() const;
            float attackmindist(int atk) const;
            float attackmaxdist(int atk) const;
            bool attackrange(int atk, float dist) const;
            bool targetable(const gameent *e) const;
            bool getsight(const vec &o, float yaw, float pitch, const vec &q, vec &v, float mdist, float fovx, float fovy) const;
            bool cansee(const vec &x, const vec &y, vec &targ = aitarget);
            bool canshoot(int atk, const gameent *e) const;
            bool canshoot(int atk) const;
            bool hastarget(int atk, const aistate &b, const gameent *e, float yaw, float pitch, float dist);
            vec getaimpos(int atk, const gameent *e);
            bool randomnode(aistate &b, const vec &pos, float guard, float wander);
            bool randomnode(aistate &b, float guard, float wander);
            bool isenemy(aistate &b, const vec &pos, float guard = sightmin, int pursue = 0);
            bool patrol(aistate &b, const vec &pos, float guard = sightmin, float wander = sightmax, int walk = 1, bool retry = false);
            bool defend(aistate &b, const vec &pos, float guard = sightmin, float wander = sightmax, int walk = 1);
            bool violence(aistate &b, const gameent *e, int pursue = 0);
            bool istarget(aistate &b, int pursue = 0, bool force = false, float mindist = 0.f);
            int isgoodammo(int gun) const;
            bool hasgoodammo() const;
            void assist(aistate &b, std::vector<interest> &interests, bool all = false, bool force = false);
            bool parseinterests(aistate &b, std::vector<interest> &interests, bool override = false, bool ignore = false);
            bool find(aistate &b, bool override = false);
            void findorientation(vec &o, float yaw, float pitch, vec &pos);
            void setup();
            bool check(aistate &b);
            int dowait(aistate &b);
            int dodefend(aistate &b);
            int dopursue(aistate &b);
            int closenode();
            int wpspot(int n, bool check = false);
            int randomlink(int n);
            bool anynode(int len = numprevnodes);
            bool hunt(aistate &b);
            void jumpto(aistate &b, const vec &pos);
            void fixfullrange(float &yaw, float &pitch, float &roll, bool full) const;
            void fixrange(float &yaw, float &pitch) const;
            void getyawpitch(const vec &from, const vec &pos, float &yaw, float &pitch);
            void scaleyawpitch(float &yaw, float &pitch, float targyaw, float targpitch, float frame, float scale);
            int process(aistate &b);
            bool hasrange(gameent *e, int weap);
            bool request(aistate &b);
            void timeouts(aistate &b);
            void logic(aistate &b, bool run);
            bool checkroute(int n);

            bool makeroute(aistate &b, int node, bool changed = true, int retries = 0);
            bool makeroute(aistate &b, const vec &pos, bool changed = true, int retries = 0);
    };

    extern avoidset obstacles;

    extern void avoid();
    extern void update();
    extern bool targetable(gameent *d, gameent *e);

    bool checkroute(gameent *d, int n);
    extern void render();
}


