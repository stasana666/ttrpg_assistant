#include <pf2e_engine/common/hash_combine.h>

size_t HashCombine(size_t h1, size_t h2)
{
    return h1 ^ (h2 << 1);
}
