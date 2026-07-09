#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "ipv67_crypto.h"

static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static uint64_t rotr64(uint64_t x, int n)
{
    return (x >> n) | (x << (64 - n));
}

static uint32_t rotr32(uint32_t x, int n)
{
    return (x >> n) | (x << (32 - n));
}

static void sha256_process_block(uint32_t state[8], const uint8_t block[64])
{
    uint32_t w[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    uint32_t t1;
    uint32_t t2;
    uint32_t s0;
    uint32_t s1;
    uint32_t ch;
    uint32_t maj;
    int i;

    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               (uint32_t)block[i * 4 + 3];
    }
    for (i = 16; i < 64; i++) {
        s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];
    f = state[5];
    g = state[6];
    h = state[7];
    for (i = 0; i < 64; i++) {
        s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        ch = (e & f) ^ (~e & g);
        t1 = h + s1 + ch + K256[i] + w[i];
        s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        maj = (a & b) ^ (a & c) ^ (b & c);
        t2 = s0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void ipv67_sha256(const uint8_t *data, size_t len, uint8_t out[IPV67_HASH256_SIZE])
{
    uint32_t state[8];
    uint8_t block[64];
    size_t full_blocks;
    size_t tail_len;
    size_t i;
    uint64_t bit_len;

    state[0] = 0x6a09e667;
    state[1] = 0xbb67ae85;
    state[2] = 0x3c6ef372;
    state[3] = 0xa54ff53a;
    state[4] = 0x510e527f;
    state[5] = 0x9b05688c;
    state[6] = 0x1f83d9ab;
    state[7] = 0x5be0cd19;
    bit_len = (uint64_t)len * 8;
    full_blocks = len / 64;
    for (i = 0; i < full_blocks; i++) sha256_process_block(state, data + i * 64);
    tail_len = len % 64;
    memset(block, 0, sizeof(block));
    if (tail_len > 0) memcpy(block, data + full_blocks * 64, tail_len);
    block[tail_len] = 0x80;
    if (tail_len >= 56) {
        sha256_process_block(state, block);
        memset(block, 0, sizeof(block));
    }
    for (i = 0; i < 8; i++) block[63 - i] = (uint8_t)(bit_len >> (i * 8));
    sha256_process_block(state, block);
    for (i = 0; i < 8; i++) {
        out[i * 4] = (uint8_t)(state[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(state[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(state[i] >> 8);
        out[i * 4 + 3] = (uint8_t)state[i];
    }
}

static const uint64_t K512[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

static void sha512_process_block(uint64_t *h0, uint64_t *h1, uint64_t *h2, uint64_t *h3,
                                  uint64_t *h4, uint64_t *h5, uint64_t *h6, uint64_t *h7,
                                  const uint8_t block[128])
{
    uint64_t w[80];
    uint64_t a, b, c, d, e, f, g, h;
    uint64_t s0, s1, t1, t2;
    int j;
    int k;

    for (j = 0; j < 16; j++) {
        w[j] = 0;
        for (k = 0; k < 8; k++)
            w[j] = (w[j] << 8) | block[j*8 + k];
    }
    for (j = 16; j < 80; j++) {
        s0 = rotr64(w[j-15], 1) ^ rotr64(w[j-15], 8) ^ (w[j-15] >> 7);
        s1 = rotr64(w[j-2], 19) ^ rotr64(w[j-2], 61) ^ (w[j-2] >> 6);
        w[j] = w[j-16] + s0 + w[j-7] + s1;
    }

    a = *h0; b = *h1; c = *h2; d = *h3;
    e = *h4; f = *h5; g = *h6; h = *h7;

    for (j = 0; j < 80; j++) {
        s1 = rotr64(e, 14) ^ rotr64(e, 18) ^ rotr64(e, 41);
        t1 = h + s1 + ((e & f) ^ (~e & g)) + K512[j] + w[j];
        s0 = rotr64(a, 28) ^ rotr64(a, 34) ^ rotr64(a, 39);
        t2 = s0 + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    *h0 += a; *h1 += b; *h2 += c; *h3 += d;
    *h4 += e; *h5 += f; *h6 += g; *h7 += h;
}

void ipv67_sha512(const uint8_t *data, size_t len, uint8_t out[IPV67_HASH_SIZE])
{
    uint64_t h0, h1, h2, h3, h4, h5, h6, h7;
    uint8_t padded[256];
    size_t i;
    size_t pad_len;
    size_t full_blocks;
    size_t tail_len;
    uint64_t bit_len;

    h0 = 0x6a09e667f3bcc908ULL;
    h1 = 0xbb67ae8584caa73bULL;
    h2 = 0x3c6ef372fe94f82bULL;
    h3 = 0xa54ff53a5f1d36f1ULL;
    h4 = 0x510e527fade682d1ULL;
    h5 = 0x9b05688c2b3e6c1fULL;
    h6 = 0x1f83d9abfb41bd6bULL;
    h7 = 0x5be0cd19137e2179ULL;

    bit_len = (uint64_t)len * 8;
    full_blocks = len / 128;

    for (i = 0; i < full_blocks; i++)
        sha512_process_block(&h0,&h1,&h2,&h3,&h4,&h5,&h6,&h7, data + i*128);

    tail_len = len % 128;
    memcpy(padded, data + full_blocks*128, tail_len);
    padded[tail_len] = 0x80;
    pad_len = tail_len + 1;

    if (pad_len <= 112) {
        memset(padded + pad_len, 0, 112 - pad_len);
        pad_len = 112;
    } else {
        memset(padded + pad_len, 0, 128 - pad_len);
        sha512_process_block(&h0,&h1,&h2,&h3,&h4,&h5,&h6,&h7, padded);
        memset(padded, 0, 112);
        pad_len = 112;
    }

    memset(padded + pad_len, 0, 8);
    for (i = 0; i < 8; i++)
        padded[pad_len + 8 + i] = (uint8_t)(bit_len >> (56 - 8*i));
    sha512_process_block(&h0,&h1,&h2,&h3,&h4,&h5,&h6,&h7, padded);

    for (i = 0; i < 8; i++) out[i]    = (uint8_t)(h0 >> (56 - 8*i));
    for (i = 0; i < 8; i++) out[8+i]  = (uint8_t)(h1 >> (56 - 8*i));
    for (i = 0; i < 8; i++) out[16+i] = (uint8_t)(h2 >> (56 - 8*i));
    for (i = 0; i < 8; i++) out[24+i] = (uint8_t)(h3 >> (56 - 8*i));
    for (i = 0; i < 8; i++) out[32+i] = (uint8_t)(h4 >> (56 - 8*i));
    for (i = 0; i < 8; i++) out[40+i] = (uint8_t)(h5 >> (56 - 8*i));
    for (i = 0; i < 8; i++) out[48+i] = (uint8_t)(h6 >> (56 - 8*i));
    for (i = 0; i < 8; i++) out[56+i] = (uint8_t)(h7 >> (56 - 8*i));
}

typedef int64_t ipv67_gf[16];

static const ipv67_gf IPV67_X25519_121665 = {0xdb41, 1};
static const uint8_t IPV67_X25519_BASE[32] = {9};

static void x25519_carry(ipv67_gf o)
{
    int i;
    int64_t c;

    for (i = 0; i < 16; i++) {
        o[i] += 1LL << 16;
        c = o[i] >> 16;
        if (i < 15) o[i + 1] += c - 1;
        else o[0] += 38 * (c - 1);
        o[i] -= c << 16;
    }
}

static void x25519_select(ipv67_gf p, ipv67_gf q, int b)
{
    int i;
    int64_t t;
    int64_t c;

    c = ~(int64_t)(b - 1);
    for (i = 0; i < 16; i++) {
        t = c & (p[i] ^ q[i]);
        p[i] ^= t;
        q[i] ^= t;
    }
}

static void x25519_unpack(ipv67_gf o, const uint8_t n[32])
{
    int i;

    for (i = 0; i < 16; i++) o[i] = n[2 * i] + ((int64_t)n[2 * i + 1] << 8);
    o[15] &= 0x7fff;
}

static void x25519_pack(uint8_t o[32], ipv67_gf n)
{
    ipv67_gf m;
    ipv67_gf t;
    int i;
    int j;
    int b;

    for (i = 0; i < 16; i++) t[i] = n[i];
    x25519_carry(t);
    x25519_carry(t);
    x25519_carry(t);
    for (j = 0; j < 2; j++) {
        m[0] = t[0] - 0xffed;
        for (i = 1; i < 15; i++) {
            m[i] = t[i] - 0xffff - ((m[i - 1] >> 16) & 1);
            m[i - 1] &= 0xffff;
        }
        m[15] = t[15] - 0x7fff - ((m[14] >> 16) & 1);
        b = (int)((m[15] >> 16) & 1);
        m[14] &= 0xffff;
        x25519_select(t, m, 1 - b);
    }
    for (i = 0; i < 16; i++) {
        o[2 * i] = (uint8_t)(t[i] & 0xff);
        o[2 * i + 1] = (uint8_t)((t[i] >> 8) & 0xff);
    }
}

static void x25519_add(ipv67_gf o, const ipv67_gf a, const ipv67_gf b)
{
    int i;

    for (i = 0; i < 16; i++) o[i] = a[i] + b[i];
}

static void x25519_sub(ipv67_gf o, const ipv67_gf a, const ipv67_gf b)
{
    int i;

    for (i = 0; i < 16; i++) o[i] = a[i] - b[i];
}

static void x25519_mul(ipv67_gf o, const ipv67_gf a, const ipv67_gf b)
{
    int64_t t[31];
    int i;
    int j;

    for (i = 0; i < 31; i++) t[i] = 0;
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 16; j++) t[i + j] += a[i] * b[j];
    }
    for (i = 0; i < 15; i++) t[i] += 38 * t[i + 16];
    for (i = 0; i < 16; i++) o[i] = t[i];
    x25519_carry(o);
    x25519_carry(o);
}

static void x25519_square(ipv67_gf o, const ipv67_gf a)
{
    x25519_mul(o, a, a);
}

static void x25519_invert(ipv67_gf o, const ipv67_gf i)
{
    ipv67_gf c;
    int a;

    for (a = 0; a < 16; a++) c[a] = i[a];
    for (a = 253; a >= 0; a--) {
        x25519_square(c, c);
        if (a != 2 && a != 4) x25519_mul(c, c, i);
    }
    for (a = 0; a < 16; a++) o[a] = c[a];
}

static void x25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32])
{
    uint8_t z[32];
    ipv67_gf x;
    ipv67_gf a;
    ipv67_gf b;
    ipv67_gf c;
    ipv67_gf d;
    ipv67_gf e;
    ipv67_gf f;
    int i;
    int r;

    for (i = 0; i < 32; i++) z[i] = scalar[i];
    z[0] &= 248;
    z[31] &= 127;
    z[31] |= 64;
    x25519_unpack(x, point);
    for (i = 0; i < 16; i++) {
        b[i] = x[i];
        d[i] = 0;
        a[i] = 0;
        c[i] = 0;
    }
    a[0] = 1;
    d[0] = 1;
    for (i = 254; i >= 0; i--) {
        r = (z[i >> 3] >> (i & 7)) & 1;
        x25519_select(a, b, r);
        x25519_select(c, d, r);
        x25519_add(e, a, c);
        x25519_sub(a, a, c);
        x25519_add(c, b, d);
        x25519_sub(b, b, d);
        x25519_square(d, e);
        x25519_square(f, a);
        x25519_mul(a, c, a);
        x25519_mul(c, b, e);
        x25519_add(e, a, c);
        x25519_sub(a, a, c);
        x25519_square(b, a);
        x25519_sub(c, d, f);
        x25519_mul(a, c, IPV67_X25519_121665);
        x25519_add(a, a, d);
        x25519_mul(c, c, a);
        x25519_mul(a, d, f);
        x25519_mul(d, b, x);
        x25519_square(b, e);
        x25519_select(a, b, r);
        x25519_select(c, d, r);
    }
    x25519_invert(c, c);
    x25519_mul(a, a, c);
    x25519_pack(out, a);
}

void ipv67_derive_public_identity(const uint8_t seed[IPV67_SEED_SIZE], uint8_t public_key[IPV67_SEED_SIZE])
{
    if (!seed || !public_key) return;
    x25519(public_key, seed, IPV67_X25519_BASE);
}

unsigned int ipv67_derive_asn_from_public(const uint8_t public_key[IPV67_SEED_SIZE])
{
    uint8_t hash[IPV67_HASH256_SIZE];
    uint32_t value;

    if (!public_key) return 0;
    ipv67_sha256(public_key, IPV67_SEED_SIZE, hash);
    value = ((uint32_t)hash[0] << 24) |
            ((uint32_t)hash[1] << 16) |
            ((uint32_t)hash[2] << 8) |
            (uint32_t)hash[3];
    return 100000u + (value % 900000u);
}

unsigned int ipv67_derive_asn(const uint8_t seed[IPV67_SEED_SIZE])
{
    uint8_t public_key[IPV67_SEED_SIZE];

    if (!seed) return 0;
    ipv67_derive_public_identity(seed, public_key);
    return ipv67_derive_asn_from_public(public_key);
}

int ipv67_load_seed(uint8_t seed[IPV67_SEED_SIZE])
{
    return ipv67_load_seed_from(IPV67_IDENTITY_KEY, seed);
}

int ipv67_save_seed(const uint8_t seed[IPV67_SEED_SIZE])
{
    return ipv67_save_seed_to(IPV67_IDENTITY_KEY, seed);
}

int ipv67_load_seed_from(const char *path, uint8_t seed[IPV67_SEED_SIZE])
{
    int fd;
    ssize_t r;

    fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    r = read(fd, seed, IPV67_SEED_SIZE);
    close(fd);
    return (r == (ssize_t)IPV67_SEED_SIZE) ? 0 : -1;
}

int ipv67_save_seed_to(const char *path, const uint8_t seed[IPV67_SEED_SIZE])
{
    int fd;
    ssize_t w;
    char dir[256];
    int len;
    int last_slash;
    int i;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        len = 0;
        while (path[len] && len < 255) len++;
        last_slash = -1;
        for (i = 0; i < len; i++) {
            if (path[i] == '/') last_slash = i;
        }
        if (last_slash > 0) {
            for (i = 0; i < last_slash && i < 255; i++) dir[i] = path[i];
            dir[last_slash] = '\0';
            mkdir(dir, 0755);
        }
        fd = open(path, O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0) return -1;
    }
    w = write(fd, seed, IPV67_SEED_SIZE);
    close(fd);
    return (w == (ssize_t)IPV67_SEED_SIZE) ? 0 : -1;
}
