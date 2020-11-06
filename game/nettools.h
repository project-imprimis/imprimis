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

inline void putint(packetbuf &p, int n)
{
    putint_(p, n);
}

inline void putuint(packetbuf &p, int n)
{
    putuint_(p, n);
}

inline void putfloat(packetbuf &p, float f)
{
    putfloat_(p, f);
}

inline void sendstring(const char *t, packetbuf &p)
{
    sendstring_(t, p);
}

