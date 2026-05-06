#include <string.h>

#include "hash_game.h"
#include "raw_game.h"

#define HASH_GAME_SIZE (1u << 18) // 262144 položek, open addressing
#define HASH_GAME_MASK (HASH_GAME_SIZE - 1)

typedef struct {
    uint8_t   used;
    edge_mask_t key;
    Dyadic    value;
} HashEntry;

static HashEntry hash_table[HASH_GAME_SIZE];

static uint64_t fnv1a_u64(uint64_t x) {
    uint64_t h = 1469598103934665603ull;
    for (int i = 0; i < 8; ++i) {
        uint8_t b = (uint8_t)(x & 0xffu);
        h ^= b;
        h *= 1099511628211ull;
        x >>= 8;
    }
    return h;
}

void hash_game_init(void) {
    memset(hash_table, 0, sizeof(hash_table));
}

int hash_game_lookup(edge_mask_t key, Dyadic *out_value) {
    uint64_t h = fnv1a_u64(key);
    uint32_t idx = (uint32_t)(h & HASH_GAME_MASK);

    for (uint32_t i = 0; i < HASH_GAME_SIZE; ++i) {
        uint32_t j = (idx + i) & HASH_GAME_MASK;
        if (!hash_table[j].used) {
            return 0; // nenalezeno
        }

        if (hash_table[j].key == key) {
            if (out_value) *out_value = hash_table[j].value;
            return 1;
        }
    }
    return 0;
}

void hash_game_insert(edge_mask_t key, Dyadic value) {
    uint64_t h = fnv1a_u64(key);
    uint32_t idx = (uint32_t)(h & HASH_GAME_MASK);

    for (uint32_t i = 0; i < HASH_GAME_SIZE; ++i) {
        uint32_t j = (idx + i) & HASH_GAME_MASK;
        if (!hash_table[j].used || (hash_table[j].key == key)) {
            hash_table[j].used = 1;
            hash_table[j].key = key;
            hash_table[j].value = value;
            return;
        }
    }
    // tabulka plná, nic moc co dělat, prostě neinsertneme
}
