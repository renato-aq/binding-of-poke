#ifndef BIND_OF_POKE_RNG_H
#define BIND_OF_POKE_RNG_H

#include <stdint.h>

typedef struct Random {
    uint32_t state;
} Random;

void random_seed(Random *random, uint32_t seed);
uint32_t random_next(Random *random);
int random_range(Random *random, int upper_bound);

#endif
