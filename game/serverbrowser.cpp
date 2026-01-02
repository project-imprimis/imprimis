#include "game.h"

struct resolverthread
{
    SDL_Thread *thread;
    const char *query;
    int starttime;
};

struct resolverresult
{
    const char *query;
    ENetAddress address;
};

std::vector<resolverthread> resolverthreads;
std::vector<const char *> resolverqueries;
std::vector<resolverresult> resolverresults;
SDL_mutex *resolvermutex;
SDL_cond *querycond, *resultcond;

static const int numresolverthreads  = 2;
static const int resolverlimit  = 3000;

//this is a void pointer because it is called by SDL_CreateThread which deals
//with a void * function
static int resolverloop(void * data)
{
    resolverthread *rt = reinterpret_cast<resolverthread *>(data);
    SDL_LockMutex(resolvermutex);
    SDL_Thread *thread = rt->thread;
    SDL_UnlockMutex(resolvermutex);
    while(thread == rt->thread)
    {
        SDL_LockMutex(resolvermutex);
        while(resolverqueries.empty())
        {
            SDL_CondWait(querycond, resolvermutex);
        }
        rt->query = resolverqueries.back();
        resolverqueries.pop_back();
        rt->starttime = totalmillis;
        SDL_UnlockMutex(resolvermutex);

        ENetAddress address = { ENET_HOST_ANY, ENET_PORT_ANY };
        enet_address_set_host(&address, rt->query);

        SDL_LockMutex(resolvermutex);
        if(rt->query && thread == rt->thread)
        {
            resolverresults.emplace_back();
            resolverresult &rr = resolverresults.back();
            rr.query = rt->query;
            rr.address = address;
            rt->query = nullptr;
            rt->starttime = 0;
            SDL_CondSignal(resultcond);
        }
        SDL_UnlockMutex(resolvermutex);
    }
    return 0;
}

static void resolverinit()
{
    resolvermutex = SDL_CreateMutex();
    querycond = SDL_CreateCond();
    resultcond = SDL_CreateCond();

    SDL_LockMutex(resolvermutex);
    for(int i = 0; i < numresolverthreads; ++i)
    {
        resolverthreads.emplace_back();
        resolverthread &rt = resolverthreads.back();
        rt.query = nullptr;
        rt.starttime = 0;
        rt.thread = SDL_CreateThread(resolverloop, "resolver", &rt);
    }
    SDL_UnlockMutex(resolvermutex);
}

static void resolverstop(resolverthread &rt)
{
    SDL_LockMutex(resolvermutex);
    if(rt.query)
    {
#if SDL_VERSION_ATLEAST(2, 0, 2)
        SDL_DetachThread(rt.thread);
#endif
        rt.thread = SDL_CreateThread(resolverloop, "resolver", &rt);
    }
    rt.query = nullptr;
    rt.starttime = 0;
    SDL_UnlockMutex(resolvermutex);
}

static void resolverclear()
{
    if(resolverthreads.empty())
    {
        return;
    }
    SDL_LockMutex(resolvermutex);
    resolverqueries.clear();
    resolverresults.clear();
    for(uint i = 0; i < resolverthreads.size(); i++)
    {
        resolverthread &rt = resolverthreads[i];
        resolverstop(rt);
    }
    SDL_UnlockMutex(resolvermutex);
}

static void resolverquery(const char *name)
{
    if(resolverthreads.empty())
    {
        resolverinit();
    }
    SDL_LockMutex(resolvermutex);
    resolverqueries.push_back(name);
    SDL_CondSignal(querycond);
    SDL_UnlockMutex(resolvermutex);
}

static bool resolvercheck(const char **name, ENetAddress *address)
{
    bool resolved = false;
    SDL_LockMutex(resolvermutex);
    if(!resolverresults.empty())
    {
        resolverresult rr = resolverresults.back();
        resolverresults.pop_back();
        *name = rr.query;
        address->host = rr.address.host;
        resolved = true;
    }
    else
    {
        for(uint i = 0; i < resolverthreads.size(); i++)
        {
            resolverthread &rt = resolverthreads[i];
            if(rt.query && totalmillis - rt.starttime > resolverlimit)
            {
                resolverstop(rt);
                *name = rt.query;
                resolved = true;
            }
        }
    }
    SDL_UnlockMutex(resolvermutex);
    return resolved;
}

bool resolverwait(const char *name, ENetAddress *address)
{
    if(resolverthreads.empty())
    {
        resolverthreads.reserve(numresolverthreads); //to avoid invalidating iterators
        resolverinit();
    }
    DEF_FORMAT_STRING(text, "resolving %s... (esc to abort)", name);
    renderprogress(0, text);

    SDL_LockMutex(resolvermutex);
    resolverqueries.push_back(name);
    SDL_CondSignal(querycond);
    int starttime = SDL_GetTicks(),
        timeout = 0;
    bool resolved = false;
    for(;;)
    {
        SDL_CondWaitTimeout(resultcond, resolvermutex, 250);
        for(uint i = 0; i < resolverresults.size(); i++)
        {
            if(resolverresults[i].query == name)
            {
                address->host = resolverresults[i].address.host;
                resolverresults.erase(resolverresults.begin() + i);
                resolved = true;
                break;
            }
        }
        if(resolved)
        {
            break;
        }
        timeout = SDL_GetTicks() - starttime;
        renderprogress(min(static_cast<float>(timeout)/resolverlimit, 1.0f), text);
        if(interceptkey(SDLK_ESCAPE))
        {
            timeout = resolverlimit + 1;
        }
        if(timeout > resolverlimit)
        {
            break;
        }
    }
    if(!resolved && timeout > resolverlimit)
    {
        for(uint i = 0; i < resolverthreads.size(); i++)
        {
            resolverthread &rt = resolverthreads[i];
            if(rt.query == name)
            {
                resolverstop(rt);
                break;
            }
        }
    }
    SDL_UnlockMutex(resolvermutex);
    return resolved;
}

static constexpr int connlimit  = 20000;

int connectwithtimeout(ENetSocket sock, const char *hostname, const ENetAddress &address)
{
    DEF_FORMAT_STRING(text, "connecting to %s... (esc to abort)", hostname);
    renderprogress(0, text);

    ENetSocketSet readset, writeset;
    if(!enet_socket_connect(sock, &address)) for(int starttime = SDL_GetTicks(), timeout = 0; timeout <= connlimit;)
    {
        ENET_SOCKETSET_EMPTY(readset);
        ENET_SOCKETSET_EMPTY(writeset);
        ENET_SOCKETSET_ADD(readset, sock);
        ENET_SOCKETSET_ADD(writeset, sock);
        int result = enet_socketset_select(sock, &readset, &writeset, 250);
        if(result < 0)
        {
            break;
        }
        else if(result > 0)
        {
            if(ENET_SOCKETSET_CHECK(readset, sock) || ENET_SOCKETSET_CHECK(writeset, sock))
            {
                int error = 0;
                if(enet_socket_get_option(sock, ENET_SOCKOPT_ERROR, &error) < 0 || error)
                {
                    break;
                }
                return 0;
            }
        }
        timeout = SDL_GetTicks() - starttime;
        renderprogress(min(static_cast<float>(timeout)/connlimit, 1.0f), text);
        if(interceptkey(SDLK_ESCAPE))
        {
            break;
        }
    }

    return -1;
}

struct pingattempts
{
    enum
    {
        Ping_MaxAttempts = 2
    };

    int offset, attempts[Ping_MaxAttempts];

    pingattempts() : offset(0) { clearattempts(); }

    void clearattempts()
    {
        memset(attempts, 0, sizeof(attempts));
    }

    void setoffset()
    {
        offset = 1 + randomint(0xFFFFFF);
    }

    int encodeping(int millis)
    {
        millis += offset;
        return millis ? millis : 1;
    }

    int decodeping(int val)
    {
        return val - offset;
    }

    int addattempt(int millis)
    {
        int val = encodeping(millis);
        for(int k = 0; k < Ping_MaxAttempts-1; ++k)
        {
            attempts[k+1] = attempts[k];
        }
        attempts[0] = val;
        return val;
    }

    bool checkattempt(int val, bool del = true)
    {
        if(val)
        {
            for(int k = 0; k < Ping_MaxAttempts; ++k)
            {
                if(attempts[k] == val)
                {
                    if(del)
                    {
                        attempts[k] = 0;
                    }
                    return true;
                }
            }
        }
        return false;
    }

};

static int currentprotocol = ProtocolVersion;

enum
{
    Resolve_Unresolved = 0,
    Resolve_Resolving,
    Resolve_Resolved
};

struct serverinfo final : servinfo, pingattempts
{
    enum
    {
        WAITING  = INT_MAX,
        MAXPINGS = 3
    };

    int resolved, lastping, nextping;
    int pings[MAXPINGS];
    ENetAddress address;
    bool keep;
    const char *password;

    serverinfo()
     : resolved(Resolve_Unresolved), keep(false), password(nullptr)
    {
        clearpings();
        setoffset();
    }

    ~serverinfo()
    {
        delete[] password;
    }

    void clearpings()
    {
        ping = WAITING;
        for(int k = 0; k < MAXPINGS; ++k)
        {
            pings[k] = WAITING;
        }
        nextping = 0;
        lastping = -1;
        clearattempts();
    }

    void cleanup()
    {
        clearpings();
        protocol = -1;
        numplayers = maxplayers = 0;
        attr.clear();
    }

    void reset()
    {
        lastping = -1;
    }

    void checkdecay(int decay)
    {
        if(lastping >= 0 && totalmillis - lastping >= decay)
        {
            cleanup();
        }
        if(lastping < 0)
        {
            lastping = totalmillis;
        }
    }

    void calcping()
    {
        int numpings   = 0,
            totalpings = 0;
        for(int k = 0; k < MAXPINGS; ++k)
        {
            if(pings[k] != WAITING)
            {
                totalpings += pings[k]; numpings++;
            }
        }
        ping = numpings ? totalpings/numpings : WAITING;
    }

    void addping(int rtt, int millis)
    {
        if(millis >= lastping)
        {
            lastping = -1;
        }
        pings[nextping] = rtt;
        nextping = (nextping+1)%MAXPINGS;
        calcping();
    }

    const char *status() const
    {
        if(address.host == ENET_HOST_ANY)
        {
            return "[unknown host]";
        }
        if(ping == WAITING)
        {
            return "[waiting for response]";
        }
        if(protocol < currentprotocol)
        {
            return "[older protocol]";
        }
        if(protocol > currentprotocol)
        {
            return "[newer protocol]";
        }
        return nullptr;
    }

    bool valid() const { return !status(); }

    static bool compare(const std::unique_ptr<serverinfo> &a, const std::unique_ptr<serverinfo> &b)
    {
        if(a->protocol == currentprotocol)
        {
            if(b->protocol != currentprotocol)
            {
                return true;
            }
        }
        else if(b->protocol == currentprotocol)
        {
            return false;
        }
        if(a->keep > b->keep)
        {
            return true;
        }
        if(a->keep < b->keep)
        {
            return false;
        }
        if(a->numplayers < b->numplayers)
        {
            return false;
        }
        if(a->numplayers > b->numplayers)
        {
            return true;
        }
        if(a->ping > b->ping)
        {
            return false;
        }
        if(a->ping < b->ping)
        {
            return true;
        }
        int cmp = strcmp(a->name, b->name);
        if(cmp != 0)
        {
            return cmp < 0;
        }
        if(a->address.port < b->address.port)
        {
            return true;
        }
        if(a->address.port > b->address.port)
        {
            return false;
        }
        return false;
    }
};

std::vector<std::unique_ptr<serverinfo>> servers;
ENetSocket pingsock = ENET_SOCKET_NULL;
int lastinfo = 0;

static std::unique_ptr<serverinfo> newserver(const char *name, int port, uint ip = ENET_HOST_ANY)
{
    std::unique_ptr<serverinfo> si = std::make_unique<serverinfo>();
    si->address.host = ip;
    si->address.port = port;
    if(ip!=ENET_HOST_ANY)
    {
        si->resolved = Resolve_Resolved;
    }
    if(name)
    {
        copystring(si->name, name);
    }
    else if(ip==ENET_HOST_ANY || enet_address_get_host_ip(&si->address, si->name, sizeof(si->name)) < 0)
    {
        return nullptr;
    }
    servers.push_back(std::move(si));
    return si;
}

void addserver(const char *name, int port, const char *password, bool keep)
{
    if(port <= 0)
    {
        port = Port_Server;
    }
    for(uint i = 0; i < servers.size(); i++)
    {
        std::unique_ptr<serverinfo> &s = servers[i];
        if(strcmp(s->name, name) || s->address.port != port)
        {
            continue;
        }
        if(password && (!s->password || strcmp(s->password, password)))
        {
            delete[] s->password;
            s->password = newstring(password);
        }
        if(keep && !s->keep)
        {
            s->keep = true;
        }
        return;
    }
    std::unique_ptr<serverinfo> s = newserver(name, port);
    if(!s)
    {
        return;
    }
    if(password)
    {
        s->password = newstring(password);
    }
    s->keep = keep;
}

static VARP(searchlan, 0, 0, 1);
static VARP(servpingrate, 1000, 5000, 60000);
static VARP(servpingdecay, 1000, 15000, 60000);
static VARP(maxservpings, 0, 10, 1000);

pingattempts lanpings;

template<size_t N>
static inline void buildping(ENetBuffer &buf, uchar (&ping)[N], pingattempts &a)
{
    ucharbuf p(ping, N);
    p.put(0xFF); p.put(0xFF);
    putint(p, a.addattempt(totalmillis));
    buf.data = ping;
    buf.dataLength = p.length();
}

static void pingservers()
{
    if(pingsock == ENET_SOCKET_NULL)
    {
        pingsock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
        if(pingsock == ENET_SOCKET_NULL)
        {
            lastinfo = totalmillis;
            return;
        }
        enet_socket_set_option(pingsock, ENET_SOCKOPT_NONBLOCK, 1);
        enet_socket_set_option(pingsock, ENET_SOCKOPT_BROADCAST, 1);

        lanpings.setoffset();
    }

    ENetBuffer buf;
    uchar ping[maxtrans];

    static uint lastping = 0;
    if(lastping >= servers.size())
    {
        lastping = 0;
    }
    for(uint i = 0; i < (maxservpings ? std::min(static_cast<int>(servers.size()), maxservpings) : servers.size()); ++i)
    {
        serverinfo &si = *servers[lastping];
        if(++lastping >= servers.size())
        {
            lastping = 0;
        }
        if(si.address.host == ENET_HOST_ANY)
        {
            continue;
        }
        buildping(buf, ping, si);
        enet_socket_send(pingsock, &si.address, &buf, 1);

        si.checkdecay(servpingdecay);
    }
    if(searchlan)
    {
        ENetAddress address;
        address.host = ENET_HOST_BROADCAST;
        address.port = Port_LanInfo;
        buildping(buf, ping, lanpings);
        enet_socket_send(pingsock, &address, &buf, 1);
    }
    lastinfo = totalmillis;
}

static void checkresolver()
{
    int resolving = 0;
    for(uint i = 0; i < servers.size(); i++)
    {
        serverinfo &si = *servers[i];
        if(si.resolved == Resolve_Resolved)
        {
            continue;
        }
        if(si.address.host == ENET_HOST_ANY)
        {
            if(si.resolved == Resolve_Unresolved)
            {
                si.resolved = Resolve_Resolving;
                resolverquery(si.name);
            }
            resolving++;
        }
    }
    if(!resolving)
    {
        return;
    }
    const char *name = nullptr;
    for(;;)
    {
        ENetAddress addr = { ENET_HOST_ANY, ENET_PORT_ANY };
        if(!resolvercheck(&name, &addr))
        {
            break;
        }
        for(uint i = 0; i < servers.size(); i++)
        {
            serverinfo &si = *servers[i];
            if(name == si.name)
            {
                si.resolved = Resolve_Resolved;
                si.address.host = addr.host;
                break;
            }
        }
    }
}

static int lastreset = 0;

static void checkpings()
{
    if(pingsock==ENET_SOCKET_NULL)
    {
        return;
    }
    enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
    ENetBuffer buf;
    ENetAddress addr;
    uchar ping[maxtrans];
    char text[maxtrans];
    buf.data = ping;
    buf.dataLength = sizeof(ping);
    while(enet_socket_wait(pingsock, &events, 0) >= 0 && events)
    {
        int len = enet_socket_receive(pingsock, &addr, &buf, 1);
        if(len <= 0)
        {
            return;
        }
        ucharbuf p(ping, len);
        int millis = getint(p);
        serverinfo *si = nullptr; //non owning pointer to unique ptr in the servers vector
        for(uint i = 0; i < servers.size(); i++)
        {
            if(addr.host == servers[i]->address.host && addr.port == servers[i]->address.port)
            {
                si = servers[i].get();
                break;
            }
        }
        if(si)
        {
            if(!si->checkattempt(millis))
            {
                continue;
            }
            millis = si->decodeping(millis);
        }
        else if(!searchlan || !lanpings.checkattempt(millis, false))
        {
            continue;
        }
        else
        {
            si = newserver(nullptr, addr.port, addr.host).get();
            millis = lanpings.decodeping(millis);
        }
        int rtt = std::clamp(totalmillis - millis, 0, min(servpingdecay, totalmillis));
        if(millis >= lastreset && rtt < servpingdecay)
        {
            si->addping(rtt, millis);
        }
        si->protocol = getint(p);
        si->numplayers = getint(p);
        si->maxplayers = getint(p);
        int numattr = getint(p);
        si->attr.clear();
        for(int j = 0; j < numattr; ++j)
        {
            int attr = getint(p);
            if(p.overread())
            {
                break;
            }
            si->attr.push_back(attr);
        }
        getstring(text, p);
        filtertext(si->map, text, false);
        getstring(text, p);
        filtertext(si->desc, text, true, true);
    }
}

static void sortservers()
{
    std::sort(servers.begin(), servers.end(), serverinfo::compare);
}
COMMAND(sortservers, "");

VARP(autosortservers, 0, 1, 1);
VARP(autoupdateservers, 0, 1, 1);

static void refreshservers()
{
    static int lastrefresh = 0;
    if(lastrefresh==totalmillis)
    {
        return;
    }
    if(totalmillis - lastrefresh > 1000)
    {
        for(uint i = 0; i < servers.size(); i++)
        {
            servers[i]->reset();
        }
        lastreset = totalmillis;
    }
    lastrefresh = totalmillis;

    checkresolver();
    checkpings();
    if(totalmillis - lastinfo >= servpingrate/(maxservpings ? std::max(1, (static_cast<int>(servers.size()) + maxservpings - 1) / maxservpings) : 1))
    {
        pingservers();
    }
    if(autosortservers)
    {
        sortservers();
    }
}

ICOMMAND(numservers, "", (), intret(servers.size()))

#define GETSERVERINFO_(idx, si, body) \
    if(static_cast<int>(servers.size()) > idx) \
    { \
        serverinfo &si = *servers[idx]; \
        body; \
    }
#define GETSERVERINFO(idx, si, body) GETSERVERINFO_(idx, si, if(si.valid()) { body; })

ICOMMAND(servinfovalid, "i", (int *i), GETSERVERINFO_(*i, si, intret(si.valid() ? 1 : 0)));
ICOMMAND(servinfodesc, "i", (int *i),
    GETSERVERINFO_(*i, si,
    {
        const char *status = si.status();
        result(status ? status : si.desc);
    }));
ICOMMAND(servinfoname, "i", (int *i), GETSERVERINFO_(*i, si, result(si.name)));
ICOMMAND(servinfoport, "i", (int *i), GETSERVERINFO_(*i, si, intret(si.address.port)));
ICOMMAND(servinfohaspassword, "i", (int *i), GETSERVERINFO_(*i, si, intret(si.password && si.password[0] ? 1 : 0)));
ICOMMAND(servinfokeep, "i", (int *i), GETSERVERINFO_(*i, si, intret(si.keep ? 1 : 0)));
ICOMMAND(servinfomap, "i", (int *i), GETSERVERINFO(*i, si, result(si.map)));
ICOMMAND(servinfoping, "i", (int *i), GETSERVERINFO(*i, si, intret(si.ping)));
ICOMMAND(servinfonumplayers, "i", (int *i), GETSERVERINFO(*i, si, intret(si.numplayers)));
ICOMMAND(servinfomaxplayers, "i", (int *i), GETSERVERINFO(*i, si, intret(si.maxplayers)));
ICOMMAND(servinfoplayers, "i", (int *i),
    GETSERVERINFO(*i, si,
    {
        if(si.maxplayers <= 0)
        {
            intret(si.numplayers);
        }
        else
        {
            result(tempformatstring(si.numplayers >= si.maxplayers ? "\f3%d/%d" : "%d/%d", si.numplayers, si.maxplayers));
        }
    }));
ICOMMAND(servinfoattr, "ii", (int *i, int *n), GETSERVERINFO(*i, si, { if(static_cast<int>(si.attr.size()) > *n) intret(si.attr[*n]); }));

ICOMMAND(connectservinfo, "is", (int *i, char *pw), GETSERVERINFO_(*i, si, connectserv(si.name, si.address.port, pw[0] ? pw : si.password)));

servinfo *getservinfo(int i)
{
    return static_cast<int>(servers.size()) > i && servers[i]->valid() ? servers[i].get() : nullptr;
}

static void clearservers(bool full = false)
{
    resolverclear();
    if(full)
    {
        servers.clear();
    }
    else
    {
        for(int i = static_cast<int>(servers.size()); --i >=0;) //note reverse iteration to preserve indices
        {
            if(!servers[i]->keep)
            {
                servers.erase(servers.begin()+i);
            }
        }
    }
}

static const int retrievelimit = 20000; //20 seconds

static void retrieveservers(std::vector<char>& data)
{
    ENetSocket sock = connectmaster(true);
    if(sock == ENET_SOCKET_NULL)
    {
        return;
    }

    extern char *mastername;
    DEF_FORMAT_STRING(text, "retrieving servers from %s... (esc to abort)", mastername);
    renderprogress(0, text);

    int starttime = SDL_GetTicks(),
        timeout   = 0;
    const char *req = "list\n";
    int reqlen = strlen(req);
    ENetBuffer buf;
    while(reqlen > 0)
    {
        enet_uint32 events = ENET_SOCKET_WAIT_SEND;
        if(enet_socket_wait(sock, &events, 250) >= 0 && events)
        {
            buf.data = (void *)req;
            buf.dataLength = reqlen;
            int sent = enet_socket_send(sock, nullptr, &buf, 1);
            if(sent < 0)
            {
                break;
            }
            req += sent;
            reqlen -= sent;
            if(reqlen <= 0)
            {
                break;
            }
        }
        timeout = SDL_GetTicks() - starttime;
        renderprogress(min(static_cast<float>(timeout)/retrievelimit, 1.0f), text);
        if(interceptkey(SDLK_ESCAPE))
        {
            timeout = retrievelimit + 1;
        }
        if(timeout > retrievelimit)
        {
            break;
        }
    }

    if(reqlen <= 0)
    {
        for(;;)
        {
            enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
            if(enet_socket_wait(sock, &events, 250) >= 0 && events)
            {
                std::array<char, 4096> databuffer;
                buf.data = databuffer.data();
                buf.dataLength = 4096;
                int recv = enet_socket_receive(sock, nullptr, &buf, 1);
                if(recv <= 0)
                {
                    break;
                }
                data.insert(data.end(), databuffer.begin(), databuffer.end());
            }
            timeout = SDL_GetTicks() - starttime;
            renderprogress(min(static_cast<float>(timeout)/retrievelimit, 1.0f), text);
            if(interceptkey(SDLK_ESCAPE))
            {
                timeout = retrievelimit + 1;
            }
            if(timeout > retrievelimit)
            {
                break;
            }
        }
    }

    if(data.size())
    {
        data.push_back('\0');
    }
    enet_socket_destroy(sock);
}

bool updatedservers = false;

static void updatefrommaster()
{
    std::vector<char> data;
    retrieveservers(data);
    if(data.empty())
    {
        conoutf("master server not replying");
    }
    else
    {
        clearservers();
        execute(data.data());
    }
    refreshservers();
    updatedservers = true;
}

static void initservers()
{
    if(autoupdateservers && !updatedservers)
    {
        updatefrommaster();
    }
}

ICOMMAND(addserver, "sis", (const char *name, int *port, const char *password), addserver(name, *port, password[0] ? password : nullptr));
ICOMMAND(keepserver, "sis", (const char *name, int *port, const char *password), addserver(name, *port, password[0] ? password : nullptr, true));
ICOMMAND(clearservers, "i", (int *full), clearservers(*full!=0));
COMMAND(updatefrommaster, "");
COMMAND(initservers, "");
COMMAND(refreshservers, "");

void writeservercfg()
{
    if(!game::savedservers())
    {
        return;
    }
    std::ofstream f;
    std::string serverpath = std::string(homedir) + std::string(game::savedservers());
    f.open(copypath(serverpath.c_str()));
    if(!f.is_open())
    {
        return;
    }
    int kept = 0;
    for(uint i = 0; i < servers.size(); i++)
    {
        const serverinfo &s = *(servers[i].get());
        if(s.keep)
        {
            if(!kept)
            {
                f << "// servers that should never be cleared from the server list\n\n";
            }
            if(s.password)
            {
                f << "keepserver " << escapeid(s.name) << " " << s.address.port << " " << escapestring(s.password) << std::endl;
            }
            else
            {
                f << "keepserver " << escapeid(s.name) << " " << s.address.port << std::endl;
            }
            kept++;
        }
    }
    if(kept)
    {
        f << std::endl;
    }
    f << "// servers connected to are added here automatically\n\n";
    for(uint i = 0; i < servers.size(); i++)
    {
        const serverinfo &s = *(servers[i].get());
        if(!s.keep)
        {
            if(s.password)
            {
                f << "addserver " << escapeid(s.name) << " " << s.address.port << " " << escapestring(s.password);
            }
            else
            {
                f << "addserver " << escapeid(s.name) << " " << s.address.port << std::endl;
            }
        }
    }
    f.close();
}

