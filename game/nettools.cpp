#include "game.h"

void ipmask::parse(const char *name)
{
    union
    {
        uchar b[sizeof(enet_uint32)];
        enet_uint32 i;
    } ipconv, maskconv;
    ipconv.i = 0;
    maskconv.i = 0;
    for(int i = 0; i < 4; ++i)
    {
        char *end = NULL;
        int n = strtol(name, &end, 10);
        if(!end)
        {
            break;
        }
        if(end > name)
        {
            ipconv.b[i] = n;
            maskconv.b[i] = 0xFF;
        }
        name = end;
        while(int c = *name)
        {
            ++name;
            if(c == '.')
            {
                break;
            }
            if(c == '/')
            {
                int range = std::clamp(static_cast<int>(strtol(name, NULL, 10)), 0, 32);
                mask = range ? ENET_HOST_TO_NET_32(0xFFffFFff << (32 - range)) : maskconv.i;
                ip = ipconv.i & mask;
                return;
            }
        }
    }
    ip = ipconv.i;
    mask = maskconv.i;
}

int ipmask::print(char *buf) const
{
    char *start = buf;
    union { uchar b[sizeof(enet_uint32)]; enet_uint32 i; } ipconv, maskconv;
    ipconv.i = ip;
    maskconv.i = mask;
    int lastdigit = -1;
    for(int i = 0; i < 4; ++i)
    {
        if(maskconv.b[i])
        {
            if(lastdigit >= 0)
            {
                *buf++ = '.';
            }
            for(int j = 0; j < i-lastdigit-1; ++j)
            {
                *buf++ = '*';
                *buf++ = '.';
            }
            buf += sprintf(buf, "%d", ipconv.b[i]);
            lastdigit = i;
        }
    }
    enet_uint32 bits = ~ENET_NET_TO_HOST_32(mask);
    int range = 32;
    for(; (bits&0xFF) == 0xFF; bits >>= 8)
    {
        range -= 8;
    }
    for(; bits&1; bits >>= 1)
    {
        --range;
    }
    if(!bits && range%8)
    {
        buf += sprintf(buf, "/%d", range);
    }
    return static_cast<int>(buf-start);
}

void putint(packetbuf &p, int n)
{
    putint_(p, n);
}

void putuint(packetbuf &p, int n)
{
    putuint_(p, n);
}

void putfloat(packetbuf &p, float f)
{
    putfloat_(p, f);
}

void sendstring(const char *t, packetbuf &p)
{
    sendstring_(t, p);
}
