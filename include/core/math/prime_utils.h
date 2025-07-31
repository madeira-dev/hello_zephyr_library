#ifndef PRIME_UTILS_H_
#define PRIME_UTILS_H_
#include "core/math/math_hal.h"

bool is_prime_mod_1_mod_2N(math_word_t q, uint32_t N);
math_word_t find_next_prime_1_mod_2N(math_word_t start, uint32_t N);

#endif
