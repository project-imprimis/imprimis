struct packetbuf : ucharbuf
{
    ENetPacket *packet;
    int growth;

    packetbuf(ENetPacket *packet) : ucharbuf(packet->data, packet->dataLength), packet(packet), growth(0) {}
    packetbuf(int growth, int pflags = 0) : growth(growth)
    {
        packet = enet_packet_create(NULL, growth, pflags);
        buf = static_cast<uchar *>(packet->data);
        maxlen = packet->dataLength;
    }
    ~packetbuf() { cleanup(); }

    void reliable()
    {
        packet->flags |= ENET_PACKET_FLAG_RELIABLE;
    }

    void resize(int n)
    {
        enet_packet_resize(packet, n);
        buf = static_cast<uchar *>(packet->data);
        maxlen = packet->dataLength;
    }

    void checkspace(int n)
    {
        if(len + n > maxlen && packet && growth > 0)
        {
            resize(max(len + n, maxlen + growth));
        }
    }

    ucharbuf subbuf(int sz)
    {
        checkspace(sz);
        return ucharbuf::subbuf(sz);
    }

    void put(const uchar &val)
    {
        checkspace(1);
        ucharbuf::put(val);
    }

    void put(const uchar *vals, int numvals)
    {
        checkspace(numvals);
        ucharbuf::put(vals, numvals);
    }

    ENetPacket *finalize()
    {
        resize(len);
        return packet;
    }

    void cleanup()
    {
        if(growth > 0 && packet && !packet->referenceCount)
        {
            enet_packet_destroy(packet);
            packet = NULL;
            buf = NULL;
            len = maxlen = 0;
        }
    }
};

struct ipmask
{
    enet_uint32 ip, mask;

    void parse(const char *name);
    int print(char *buf) const;
    bool check(enet_uint32 host) const { return (host & mask) == ip; }
};

extern void putint(packetbuf &p, int n);
extern void putuint(packetbuf &p, int n);
extern void putfloat(packetbuf &p, float f);
extern void sendstring(const char *t, packetbuf &p);

