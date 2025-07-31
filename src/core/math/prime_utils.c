#include "prime_utils.h"

// fast trial primality
static bool is_prime_simple(math_word_t x)
{
    if (x < 2)
        return false;
    if (x % 2 == 0)
        return x == 2;
    for (math_word_t i = 3; i * i <= x; i += 2)
    {
        if (x % i == 0)
            return false;
    }
    return true;
}

bool is_prime_mod_1_mod_2N(math_word_t q, uint32_t N)
{
    return (q % (2 * N) == 1) && is_prime_simple(q);
}

math_word_t find_next_prime_1_mod_2N(math_word_t start, uint32_t N)
{
    math_word_t mod = 2 * N;
    math_word_t rem = start % mod;
    math_word_t candidate = (rem == 1) ? start : start + (mod + 1 - rem);
    while (!is_prime_simple(candidate))
    {
        candidate += mod;
    }
    return candidate;
}
