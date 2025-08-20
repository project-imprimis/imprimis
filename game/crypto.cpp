#include "../libprimis-headers/cube.h"
#include "game.h"

// --------------------------------------------------------------
// Cryptography Module: Tiger Hash Function + ECC, improved
// --------------------------------------------------------------

namespace tiger
{
    constexpr int tigerpasses = 3;
    typedef unsigned long long chunk; // 64-bit basic unit

    union hashval
    {
        uchar bytes[24];    // 192 bits = 24 bytes
        chunk chunks[3];    // three 64-bit words
    };

    // S-boxes: fixed large lookup tables, initialized dynamically
    extern chunk sboxes[4*256];

    /** Initialize Tiger S-boxes, must be called before first use. */
    void gensboxes();

    /** Tiger compression function — core permutation step. */
    void compress(const chunk *str, chunk state[3]);

    /** Compute Tiger hash of given input. */
    void hash(const uchar *data, int length, hashval &val);

    // ---- helper functions ----

    /** Initialize S-boxes on first use (thread-safety not guaranteed). */
    static inline void ensure_sboxes()
    {
        static bool init = false;
        if(!init) { gensboxes(); init = true; }
    }

    /** Convenience wrapper: Tiger hash of arbitrary data. */
    static inline hashval hash(const uchar *data, int length)
    {
        ensure_sboxes();
        hashval val;
        hash(data, length, val);
        return val;
    }

    /** Convenience wrapper: Tiger hash of std::string-compatible. */
    template<typename S>
    static inline hashval hash(const S &str)
    {
        return hash(reinterpret_cast<const uchar *>(str.data()), int(str.size()));
    }

    /** Convert Tiger hash to lower-case hexadecimal using little-endian byte order (standard). */
    inline std::string tohex(const hashval &v)
    {
        char buf[49];
        for(int i = 0; i < 3*8; ++i)
        {
            uchar c = v.bytes[i];
            buf[2*i]     = "0123456789abcdef"[c>>4];
            buf[2*i + 1] = "0123456789abcdef"[c&0xF];
        }
        buf[48] = 0;
        return buf;
    }

    /** Compare two Tiger hashes for equality. */
    inline bool equals(const hashval &a, const hashval &b)
    {
        for(int i = 0; i < 3; ++i) if(a.chunks[i] != b.chunks[i]) return false;
        return true;
    }

    /** Print Tiger hash (useful for debug). */
    inline void print(const hashval &v, stream *out)
    {
        std::string s = tohex(v);
        out->write(s.c_str(), s.size());
    }
}

// instantiate the static S-box tables
tiger::chunk tiger::sboxes[4*256] = {0};

// --------------------------------------------------------------
// Tiger S-Box generation - same as ref implementation
// --------------------------------------------------------------

void tiger::gensboxes()
{
    const char *msg = "Tiger - A Fast New Hash Function, by Ross Anderson and Eli Biham";
    chunk state[3] = { 0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL, 0xF096A5B4C3B2E187ULL };
    uchar temp[64];
    memset(temp, 0, 64);
    memcpy(temp, msg, std::min<int>(strlen(msg), 64));

    // init sboxes with sequential bytes
    for(int i = 0; i < 1024; ++i)
    {
        reinterpret_cast<uchar*>(&sboxes[i])[0] = i & 0xFF;
        reinterpret_cast<uchar*>(&sboxes[i])[1] = (i >> 8) & 0xFF;
        reinterpret_cast<uchar*>(&sboxes[i])[2] = (i >> 16) & 0xFF;
        reinterpret_cast<uchar*>(&sboxes[i])[3] = (i >> 24) & 0xFF;
        reinterpret_cast<uchar*>(&sboxes[i])[4] = (i >> 32) & 0xFF;
        reinterpret_cast<uchar*>(&sboxes[i])[5] = (i >> 40) & 0xFF;
        reinterpret_cast<uchar*>(&sboxes[i])[6] = (i >> 48) & 0xFF;
        reinterpret_cast<uchar*>(&sboxes[i])[7] = (i >> 56) & 0xFF;
    }

    int abc = 2;
    for(int pass = 0; pass < 5; pass++)
    {
        for(int i = 0; i < 256; i++)
        {
            for(int sb = 0; sb < 1024; sb += 256)
            {
                abc++;
                if(abc >= 3)
                {
                    abc = 0;
                    compress(reinterpret_cast<chunk*>(temp), state);
                }
                for(int j = 0; j < 8; ++j)
                {
                    uchar t = reinterpret_cast<uchar*>(&sboxes[sb+i])[j];
                    uchar s = reinterpret_cast<uchar*>(&state[abc])[j];
                    std::swap(reinterpret_cast<uchar*>(&sboxes[sb+i])[j],
                              reinterpret_cast<uchar*>(&sboxes[sb+s])[j]);
                }
            }
        }
    }
}

// compress() implementation unchanged, already present

// Tiger hash — unchanged, already present (call gensboxes at start)
void tiger::hash(const uchar *str, int length, hashval &val)
{
    ensure_sboxes();

    val.chunks[0] = 0x0123456789ABCDEFULL;
    val.chunks[1] = 0xFEDCBA9876543210ULL;
    val.chunks[2] = 0xF096A5B4C3B2E187ULL;

    while(length >= 64)
    {
        compress(reinterpret_cast<const chunk*>(str), val.chunks);
        str += 64;
        length -= 64;
    }
    uchar temp[64];
    memcpy(temp, str, length);
    temp[length] = 0x01;
    memset(temp + length + 1, 0, 64-length-1);
    if(length + 1 > 56)
    {
        compress(reinterpret_cast<chunk*>(temp), val.chunks);
        memset(temp, 0, 56);
    }
    else memset(temp + length + 1, 0, 56 - (length +1));
    *(reinterpret_cast<chunk*>(temp + 56)) = static_cast<chunk>(length) << 3;
    compress(reinterpret_cast<chunk*>(temp), val.chunks);
}

// ========================== ECC ENHANCEMENTS =========================

// Predeclare
struct ecjacobian;

// Generate compressed pubkey (x + sign bit)
inline void compress_pubkey(const ecjacobian &point, std::vector<char> &out);

// Add convenience wrapper for private key scalar -> public key
template<int BI_DIGITS>
inline void scalar_base_mult(const bigint<BI_DIGITS> &priv, std::vector<char> &out)
{
    ecjacobian p(ecjacobian::base);
    p.mul(priv);
    p.normalize();
    compress_pubkey(p, out);
    out.push_back('\0');
}

/** ECC point compression: outputs `+` or `-` prefix then x coordinate as hex */
inline void compress_pubkey(const ecjacobian &point, std::vector<char> &out)
{
    auto p = point;
    p.normalize();
    out.push_back(p.y.hasbit(0) ? '-' : '+');
    p.x.printdigits(out);
}

/** Generate keypair from seed string */
inline void generate_keypair(const char *seed, std::vector<char> &privstr, std::vector<char> &pubstr)
{
    auto hashval = tiger::hash(reinterpret_cast<const uchar*>(seed), int(strlen(seed)));
    bigint<24*8/bidigitbits> priv;
    memcpy(priv.digits, hashval.bytes, sizeof(hashval.bytes));
    priv.len = 8*sizeof(hashval.bytes)/bidigitbits;
    priv.shrink();
    priv.printdigits(privstr);
    privstr.push_back('\0';
    );

    scalar_base_mult(priv, pubstr);
}

/** Verify if an EC point lies on P-192 curve */
inline bool is_valid_point(const gfield &x, const gfield &y)
{
    gfield left, right,tmp;
    left.square(y);
    right.square(x).mul(x).sub(tmp.add(x,x).add(x)).add(ecjacobian::B);
    return left == right;
}

/** Convert Tiger hash of arbitrary string into hex string (for e.g. authentication) */
inline std::string tiger_string_hex(const char *s)
{
    return tiger::tohex(tiger::hash(reinterpret_cast<const uchar*>(s),(int)strlen(s)));
}

/** Short answer-challenge function using privkey string and challenge string */
inline void compute_shared_secret(const std::string &privstr, const std::string &challenge, std::vector<char> &shared)
{
    gfint privkey;
    privkey.parse(privstr.c_str());
    ecjacobian ch;
    ch.parse(challenge.c_str());
    ch.mul(privkey);
    ch.normalize();
    ch.x.printdigits(shared);
    shared.push_back('\0');
}
