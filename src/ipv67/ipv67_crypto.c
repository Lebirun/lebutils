#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "ipv67_crypto.h"

static uint64_t rotr64(uint64_t x, int n)
{
    return (x >> n) | (x << (64 - n));
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

static const char BASE36_CHARS[37] = "abcdefghijklmnopqrstuvwxyz0123456789";

static void encode_domain(const uint8_t *hash_bytes, int start, int end, char *out, int out_len)
{
    uint64_t val;
    int j;
    int n;

    val = 0;
    for (j = start; j < end; j++)
        val = (val << 8) | hash_bytes[j];

    n = out_len - 1;
    while (n >= 0) {
        out[n] = BASE36_CHARS[val % 36];
        val /= 36;
        n--;
    }
}

void ipv67_derive_addr(const uint8_t seed[IPV67_SEED_SIZE], char addr[IPV67_ADDR_STR_MAX])
{
    uint8_t hash[IPV67_HASH_SIZE];
    int i;
    int pos;
    uint32_t v;
    char domain_part1[17];
    char domain_part2[17];

    ipv67_sha512(seed, IPV67_SEED_SIZE, hash);

    pos = 0;

    for (i = 0; i < 3; i++) {
        v = hash[i];
        addr[pos++] = "0123456789abcdef"[v >> 4];
        addr[pos++] = "0123456789abcdef"[v & 0xf];
    }
    addr[pos++] = '.';

    for (i = 3; i < 6; i++) {
        v = hash[i];
        addr[pos++] = "0123456789abcdef"[v >> 4];
        addr[pos++] = "0123456789abcdef"[v & 0xf];
    }
    addr[pos++] = '.';

    encode_domain(hash, 6, 14, domain_part1, 16);
    encode_domain(hash, 14, 22, domain_part2, 16);
    memcpy(addr + pos, domain_part1, 16);
    pos += 16;
    memcpy(addr + pos, domain_part2, 16);
    pos += 16;
    addr[pos++] = '.';

    for (i = 22; i < 25; i++) {
        v = hash[i];
        addr[pos++] = "0123456789abcdef"[v >> 4];
        addr[pos++] = "0123456789abcdef"[v & 0xf];
    }
    addr[pos++] = '.';

    for (i = 25; i < 28; i++) {
        v = hash[i];
        addr[pos++] = "0123456789abcdef"[v >> 4];
        addr[pos++] = "0123456789abcdef"[v & 0xf];
    }
    addr[pos] = '\0';
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
