#ifndef IPV67_CRYPTO_H
#define IPV67_CRYPTO_H

#include <stdint.h>

#define IPV67_SEED_SIZE   32
#define IPV67_HASH_SIZE   64

#define IPV67_ZONE_MAX    6
#define IPV67_DOMAIN_MAX  32
#define IPV67_NODE_MAX    6
#define IPV67_ADDR_STR_MAX  (IPV67_ZONE_MAX + 1 + IPV67_ZONE_MAX + 1 + IPV67_DOMAIN_MAX + 1 + IPV67_NODE_MAX + 1 + IPV67_NODE_MAX + 1)

#define IPV67_IDENTITY_DIR  "/etc/ipv67"
#define IPV67_IDENTITY_KEY  "/etc/ipv67/identity.key"

void ipv67_sha512(const uint8_t *data, size_t len, uint8_t out[IPV67_HASH_SIZE]);
void ipv67_derive_addr(const uint8_t seed[IPV67_SEED_SIZE], char addr[IPV67_ADDR_STR_MAX]);
int  ipv67_load_seed(uint8_t seed[IPV67_SEED_SIZE]);
int  ipv67_save_seed(const uint8_t seed[IPV67_SEED_SIZE]);
int  ipv67_load_seed_from(const char *path, uint8_t seed[IPV67_SEED_SIZE]);
int  ipv67_save_seed_to(const char *path, const uint8_t seed[IPV67_SEED_SIZE]);

#endif
