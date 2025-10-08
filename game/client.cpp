// client.cpp, mostly network related client game code

#include "game.h"

ENetHost *clienthost = NULL;
ENetPeer *curpeer = NULL,
         *connpeer = NULL;
int connmillis = 0,
    connattempts = 0,
    discmillis = 0;

SVAR(connectservmsg, "");
SVAR(connectservattempt, "");
SVAR(connectservfailed, "");
SVAR(connectservlan, "");
SVAR(connectservnoconnect, "");
static SVAR(reconnectnone, "");
static SVAR(disconnectmessage, "");
static SVAR(trydisconnectabort, "");
static SVAR(trydisconnectattempt, "");
static SVAR(trydisconnectnoconnect, "");
static SVAR(neterrmessage, "");
static SVAR(gets2cattempt, "");
static SVAR(gets2cfailed, "");
static SVAR(gets2cconnect, "");
static SVAR(gets2cdisconnect, "");
static SVAR(gets2cneterr, "");

void setrate(int rate)
{
   if(!curpeer)
   {
       return;
   }
   enet_host_bandwidth_limit(clienthost, rate*1024, rate*1024);
}

VARF(rate, 0, 0, 1024, setrate(rate));

void throttle();

VARF(throttleinterval, 0, 5, 30, throttle());
VARF(throttleaccel,    0, 2, 32, throttle());
VARF(throttledecel,    0, 2, 32, throttle());

void throttle()
{
    if(!curpeer)
    {
        return;
    }
    enet_peer_throttle_configure(curpeer, throttleinterval*1000, throttleaccel, throttledecel);
}

const ENetAddress *connectedpeer()
{
    return curpeer ? &curpeer->address : NULL;
}

ICOMMAND(connectedip, "", (),
{
    const ENetAddress *address = connectedpeer();
    string hostname;
    result(address && enet_address_get_host_ip(address, hostname, sizeof(hostname)) >= 0 ? hostname : "");
});

ICOMMAND(connectedport, "", (),
{
    const ENetAddress *address = connectedpeer();
    intret(address ? address->port : -1);
});

void abortconnect()
{
    if(!connpeer)
    {
        return;
    }
    game::connectfail();
    if(connpeer->state!=ENET_PEER_STATE_DISCONNECTED)
    {
        enet_peer_reset(connpeer);
    }
    connpeer = NULL;
    if(curpeer)
    {
        return;
    }
    enet_host_destroy(clienthost);
    clienthost = NULL;
}

static SVARP(connectname, "");
static VARP(connectport, 0, 0, 0xFFFF);

void connectserv(const char *servername, int serverport, const char *serverpassword)
{
    if(connpeer)
    {
        conoutf("%s", connectservmsg);
        abortconnect();
    }
    if(serverport <= 0)
    {
        serverport = Port_Server;
    }
    ENetAddress address;
    address.port = serverport;

    if(servername)
    {
        if(strcmp(servername, connectname))
        {
            setsvar("connectname", servername);
        }
        if(serverport != connectport)
        {
            setvar("connectport", serverport);
        }
        addserver(servername, serverport, serverpassword && serverpassword[0] ? serverpassword : NULL);
        conoutf("%s %s:%d", connectservattempt, servername, serverport);
        if(!resolverwait(servername, &address))
        {
            conoutf("%s %s", connectservfailed, servername);
            return;
        }
    }
    else
    {
        setsvar("connectname", "");
        setvar("connectport", 0);
        conoutf("%s", connectservlan);
        address.host = ENET_HOST_BROADCAST;
    }

    if(!clienthost)
    {
        clienthost = enet_host_create(NULL, 2, server::numchannels(), rate*1024, rate*1024);
        if(!clienthost)
        {
            conoutf("%s", connectservnoconnect);
            return;
        }
        clienthost->duplicatePeers = 0;
    }

    connpeer = enet_host_connect(clienthost, &address, server::numchannels(), 0);
    enet_host_flush(clienthost);
    connmillis = totalmillis;
    connattempts = 0;

    game::connectattempt(servername ? servername : "", serverpassword ? serverpassword : "", address);
}

void reconnect(const char *serverpassword)
{
    if(!connectname[0] || connectport <= 0)
    {
        conoutf(Console_Error, "%s", reconnectnone);
        return;
    }

    connectserv(connectname, connectport, serverpassword);
}

void disconnect(bool async, bool cleanup)
{
    if(curpeer)
    {
        if(!discmillis)
        {
            enet_peer_disconnect(curpeer, Discon_None);
            enet_host_flush(clienthost);
            discmillis = totalmillis;
        }
        if(curpeer->state!=ENET_PEER_STATE_DISCONNECTED)
        {
            if(async)
            {
                return;
            }
            enet_peer_reset(curpeer);
        }
        curpeer = NULL;
        discmillis = 0;
        conoutf("%s", disconnectmessage);
        game::gamedisconnect(cleanup);
        mainmenu = 1;
        execident("resethud");
    }
    if(!connpeer && clienthost)
    {
        enet_host_destroy(clienthost);
        clienthost = NULL;
    }
}

static void trydisconnect()
{

    if(connpeer)
    {
        conoutf("%s", trydisconnectabort);
        abortconnect();
    }
    else if(curpeer)
    {
        conoutf("%s", trydisconnectattempt);
        disconnect(!discmillis);
    }
    else conoutf("%s", trydisconnectnoconnect);
}

ICOMMAND(connect, "sis", (char *name, int *port, char *pw), connectserv(name, *port, pw));
ICOMMAND(lanconnect, "is", (int *port, char *pw), connectserv(NULL, *port, pw));
COMMAND(reconnect, "s");
ICOMMAND(disconnect, "", (), trydisconnect());

void sendclientpacket(ENetPacket *packet, int chan)
{
    if(curpeer)
    {
        enet_peer_send(curpeer, chan, packet);
    }
}

void flushclient()
{
    if(clienthost)
    {
        enet_host_flush(clienthost);
    }
}

void neterr(const char *s, bool disc)
{
    conoutf(Console_Error, "%s (%s)", neterrmessage, s);
    if(disc)
    {
        disconnect();
    }
}

void localservertoclient(int chan, ENetPacket *packet)   // processes any updates from the server
{
    packetbuf p(packet);
    game::parsepacketclient(chan, p);
}

void gets2c()           // get updates from the server
{
    ENetEvent event;
    if(!clienthost)
    {
        return;
    }
    if(connpeer && totalmillis/3000 > connmillis/3000)
    {
        conoutf("%s", gets2cattempt);
        connmillis = totalmillis;
        ++connattempts;
        if(connattempts > 3)
        {
            conoutf("%s", gets2cfailed);
            abortconnect();
            return;
        }
    }
    while(clienthost && enet_host_service(clienthost, &event, 0)>0)
    {
        switch(event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
            {
                disconnect(false, false);
                curpeer = connpeer;
                connpeer = NULL;
                conoutf("%s", gets2cconnect);
                throttle();
                if(rate)
                {
                    setrate(rate);
                }
                game::gameconnect(true);
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE:
            {
                if(discmillis)
                {
                    conoutf("%s", gets2cdisconnect);
                }
                else
                {
                    localservertoclient(event.channelID, event.packet);
                }
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
            {
                if(event.data >= Discon_NumDiscons)
                {
                    event.data = Discon_None;
                }
                if(event.peer==connpeer)
                {
                    conoutf("%s", gets2cfailed);
                    abortconnect();
                }
                else
                {
                    if(!discmillis || event.data)
                    {
                        const char *msg = disconnectreason(event.data);
                        if(msg)
                        {
                            conoutf("%s (%s)", gets2cneterr, msg);
                        }
                        else
                        {
                            conoutf("%s", gets2cneterr);
                        }
                    }
                    disconnect();
                }
                return;
            }
            default:
            {
                break;
            }
        }
    }
}

