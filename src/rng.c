#include "rng.h"

void random_seed(Random *random, uint32_t seed)
{
    random->state = seed == 0U ? 0x6d2b79f5U : seed;
}

uint32_t random_next(Random *random)
{
    uint32_t value = random->state;
    value ^= value << 13U;
    value ^= value >> 17U;
    value ^= value << 5U;
    random->state = value;
    return value;
}

int random_range(Random *random, int upper_bound)
{
    if (upper_bound <= 0) {
        return 0;
    }

    uint32_t bound = (uint32_t)upper_bound;
    uint32_t threshold = (uint32_t)(-bound) % bound;
    uint32_t value;
    do {
        value = random_next(random);
    } while (value < threshold);
    return (int)(value % bound);
}
