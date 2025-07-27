#include "core/math/biginteger.h"
#include "core/math/math_hal.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(biginteger, LOG_LEVEL_DBG);

// ============================================================================
// Initialization and Basic Operations
// ============================================================================

void bigint_init(bigint_t *a)
{
    if (!a)
        return;

    memset(a->words, 0, sizeof(a->words));
    a->word_count = 1;
    a->is_negative = false;
}

void bigint_init_u64(bigint_t *a, uint64_t value)
{
    if (!a)
        return;

    bigint_init(a);

#ifdef CONFIG_SOC_NRF52840
    // For 32-bit words, split the 64-bit value
    a->words[0] = (uint32_t)(value & 0xFFFFFFFF);
    a->words[1] = (uint32_t)(value >> 32);

    if (a->words[1] != 0)
    {
        a->word_count = 2;
    }
    else
    {
        a->word_count = 1;
    }
#else
    // For 64-bit words, direct assignment
    a->words[0] = value;
    a->word_count = (value == 0) ? 1 : 1;
#endif
}

int bigint_init_string(bigint_t *a, const char *hex_string)
{
    if (!a || !hex_string)
        return -1;

    bigint_init(a);

    size_t len = strlen(hex_string);
    if (len == 0)
        return 0;

    // Simple hex parsing (basic implementation)
    for (size_t i = 0; i < len && i < sizeof(a->words) * 2; i++)
    {
        char c = hex_string[len - 1 - i];
        uint8_t nibble;

        if (c >= '0' && c <= '9')
        {
            nibble = c - '0';
        }
        else if (c >= 'a' && c <= 'f')
        {
            nibble = c - 'a' + 10;
        }
        else if (c >= 'A' && c <= 'F')
        {
            nibble = c - 'A' + 10;
        }
        else
        {
            return -1; // Invalid character
        }

        size_t word_idx = i / (sizeof(bigint_word_t) * 2);
        size_t bit_pos = (i % (sizeof(bigint_word_t) * 2)) * 4;

        if (word_idx < BIGINT_MAX_WORDS)
        {
            a->words[word_idx] |= ((bigint_word_t)nibble) << bit_pos;
            if (word_idx >= a->word_count)
            {
                a->word_count = word_idx + 1;
            }
        }
    }

    bigint_normalize(a);
    return 0;
}

void bigint_copy(bigint_t *dest, const bigint_t *src)
{
    if (!dest || !src)
        return;

    memcpy(dest->words, src->words, sizeof(dest->words));
    dest->word_count = src->word_count;
    dest->is_negative = src->is_negative;
}

void bigint_zero(bigint_t *a)
{
    if (!a)
        return;

    memset(a->words, 0, sizeof(a->words));
    a->word_count = 1;
    a->is_negative = false;
}

bool bigint_is_zero(const bigint_t *a)
{
    if (!a)
        return true;

    for (uint32_t i = 0; i < a->word_count; i++)
    {
        if (a->words[i] != 0)
            return false;
    }
    return true;
}

bool bigint_is_one(const bigint_t *a)
{
    if (!a || a->is_negative)
        return false;

    if (a->words[0] != 1)
        return false;

    for (uint32_t i = 1; i < a->word_count; i++)
    {
        if (a->words[i] != 0)
            return false;
    }
    return true;
}

// ============================================================================
// Comparison Operations
// ============================================================================

int bigint_compare(const bigint_t *a, const bigint_t *b)
{
    if (!a || !b)
        return 0;

    // Handle signs
    if (a->is_negative && !b->is_negative)
        return -1;
    if (!a->is_negative && b->is_negative)
        return 1;

    // Both same sign, compare magnitudes
    int sign = a->is_negative ? -1 : 1;

    if (a->word_count > b->word_count)
        return sign;
    if (a->word_count < b->word_count)
        return -sign;

    // Same word count, compare from most significant
    for (int i = a->word_count - 1; i >= 0; i--)
    {
        if (a->words[i] > b->words[i])
            return sign;
        if (a->words[i] < b->words[i])
            return -sign;
    }

    return 0; // Equal
}

bool bigint_equals(const bigint_t *a, const bigint_t *b)
{
    return bigint_compare(a, b) == 0;
}

bool bigint_less_than(const bigint_t *a, const bigint_t *b)
{
    return bigint_compare(a, b) < 0;
}

// ============================================================================
// Arithmetic Operations
// ============================================================================

int bigint_add(bigint_t *result, const bigint_t *a, const bigint_t *b)
{
    if (!result || !a || !b)
        return -1;

    // Handle signs
    if (a->is_negative == b->is_negative)
    {
        // Same sign: add magnitudes
        bigint_word_t carry = 0;
        uint32_t max_words = (a->word_count > b->word_count) ? a->word_count : b->word_count;

        if (max_words >= BIGINT_MAX_WORDS)
            return -1; // Overflow

        for (uint32_t i = 0; i < max_words || carry; i++)
        {
            if (i >= BIGINT_MAX_WORDS)
                return -1; // Overflow

            bigint_word_t sum = carry;
            if (i < a->word_count)
                sum += a->words[i];
            if (i < b->word_count)
                sum += b->words[i];

            result->words[i] = sum;
            carry = (sum < carry) ? 1 : 0; // Detect overflow

#ifdef CONFIG_SOC_NRF52840
            // For 32-bit words, check if sum overflowed
            if (i < a->word_count && i < b->word_count)
            {
                uint64_t full_sum = (uint64_t)a->words[i] + b->words[i] + carry;
                result->words[i] = (uint32_t)full_sum;
                carry = full_sum >> 32;
            }
#endif
        }

        result->word_count = max_words;
        if (carry && max_words < BIGINT_MAX_WORDS)
        {
            result->words[max_words] = carry;
            result->word_count++;
        }

        result->is_negative = a->is_negative;
    }
    else
    {
        // Different signs: subtract magnitudes
        if (bigint_compare(a, b) >= 0)
        {
            return bigint_sub(result, a, b);
        }
        else
        {
            int ret = bigint_sub(result, b, a);
            result->is_negative = !result->is_negative;
            return ret;
        }
    }

    bigint_normalize(result);
    return 0;
}

int bigint_sub(bigint_t *result, const bigint_t *a, const bigint_t *b)
{
    if (!result || !a || !b)
        return -1;

    // For simplicity, assume a >= b (magnitude)
    if (bigint_compare(a, b) < 0)
        return -1;

    bigint_word_t borrow = 0;

    for (uint32_t i = 0; i < a->word_count; i++)
    {
        bigint_word_t minuend = a->words[i];
        bigint_word_t subtrahend = (i < b->word_count) ? b->words[i] : 0;

        if (minuend < subtrahend + borrow)
        {
            // Need to borrow
#ifdef CONFIG_SOC_NRF52840
            result->words[i] = (uint32_t)(0x100000000ULL + minuend - subtrahend - borrow);
#else
            result->words[i] = (uint64_t)(0x10000000000000000ULL + minuend - subtrahend - borrow);
#endif
            borrow = 1;
        }
        else
        {
            result->words[i] = minuend - subtrahend - borrow;
            borrow = 0;
        }
    }

    result->word_count = a->word_count;
    result->is_negative = false;

    bigint_normalize(result);
    return 0;
}

int bigint_mult(bigint_t *result, const bigint_t *a, const bigint_t *b)
{
    if (!result || !a || !b)
        return -1;

    // Clear result
    bigint_zero(result);

    if (bigint_is_zero(a) || bigint_is_zero(b))
        return 0;

    // Check for overflow
    if (a->word_count + b->word_count > BIGINT_MAX_WORDS)
        return -1;

    // Grade school multiplication
    for (uint32_t i = 0; i < a->word_count; i++)
    {
        bigint_word_t carry = 0;

        for (uint32_t j = 0; j < b->word_count; j++)
        {
            if (i + j >= BIGINT_MAX_WORDS)
                return -1;

#ifdef CONFIG_SOC_NRF52840
            uint64_t prod = (uint64_t)a->words[i] * b->words[j] + result->words[i + j] + carry;
            result->words[i + j] = (uint32_t)prod;
            carry = prod >> 32;
#else
            // For 64-bit, we'd need 128-bit arithmetic, simplified here
            uint64_t prod = a->words[i] * b->words[j] + result->words[i + j] + carry;
            result->words[i + j] = prod; // May overflow, simplified
            carry = 0;                   // Simplified
#endif
        }

        if (carry && i + b->word_count < BIGINT_MAX_WORDS)
        {
            result->words[i + b->word_count] = carry;
        }
    }

    result->word_count = a->word_count + b->word_count;
    result->is_negative = (a->is_negative != b->is_negative);

    bigint_normalize(result);
    return 0;
}

int bigint_mult_word(bigint_t *result, const bigint_t *a, math_word_t word)
{
    if (!result || !a)
        return -1;

    bigint_zero(result);

    if (word == 0 || bigint_is_zero(a))
        return 0;

    math_word_t carry = 0;

    for (uint32_t i = 0; i < a->word_count; i++)
    {
        if (i >= BIGINT_MAX_WORDS)
            return -1;

#ifdef CONFIG_SOC_NRF52840
        uint64_t prod = (uint64_t)a->words[i] * word + carry;
        result->words[i] = (uint32_t)prod;
        carry = prod >> 32;
#else
        // Simplified for 64-bit
        uint64_t prod = a->words[i] * word + carry;
        result->words[i] = prod;
        carry = 0; // Simplified
#endif
    }

    result->word_count = a->word_count;
    if (carry && result->word_count < BIGINT_MAX_WORDS)
    {
        result->words[result->word_count] = carry;
        result->word_count++;
    }

    result->is_negative = a->is_negative;
    bigint_normalize(result);
    return 0;
}

int bigint_div_word(bigint_t *quotient, math_word_t *remainder,
                    const bigint_t *a, math_word_t word)
{
    if (!a || word == 0)
        return -1;

    if (quotient)
        bigint_zero(quotient);
    if (remainder)
        *remainder = 0;

    if (bigint_is_zero(a))
        return 0;

    math_word_t rem = 0;

    // Long division from most significant word
    for (int i = a->word_count - 1; i >= 0; i--)
    {
#ifdef CONFIG_SOC_NRF52840
        uint64_t dividend = ((uint64_t)rem << 32) | a->words[i];
        if (quotient)
        {
            quotient->words[i] = (uint32_t)(dividend / word);
        }
        rem = dividend % word;
#else
        // Simplified for 64-bit
        if (quotient)
        {
            quotient->words[i] = a->words[i] / word;
        }
        rem = a->words[i] % word;
#endif
    }

    if (quotient)
    {
        quotient->word_count = a->word_count;
        quotient->is_negative = a->is_negative;
        bigint_normalize(quotient);
    }

    if (remainder)
        *remainder = rem;

    return 0;
}

// ============================================================================
// Utility Functions
// ============================================================================

void bigint_normalize(bigint_t *a)
{
    if (!a)
        return;

    // Remove leading zeros
    while (a->word_count > 1 && a->words[a->word_count - 1] == 0)
    {
        a->word_count--;
    }

    // If result is zero, clear negative flag
    if (bigint_is_zero(a))
    {
        a->is_negative = false;
    }
}

bool bigint_is_valid(const bigint_t *a)
{
    if (!a)
        return false;
    if (a->word_count == 0 || a->word_count > BIGINT_MAX_WORDS)
        return false;
    return true;
}

int bigint_to_string(const bigint_t *a, char *buffer, size_t buffer_size)
{
    if (!a || !buffer || buffer_size < 3)
        return -1;

    if (bigint_is_zero(a))
    {
        strcpy(buffer, "0");
        return 0;
    }

    // Simple implementation: just show first word in hex
    if (a->is_negative)
    {
        snprintf(buffer, buffer_size, "-%x", (unsigned int)a->words[0]);
    }
    else
    {
        snprintf(buffer, buffer_size, "%x", (unsigned int)a->words[0]);
    }

    return 0;
}

uint32_t bigint_bit_count(const bigint_t *a)
{
    if (!a || bigint_is_zero(a))
        return 0;

    uint32_t bits = (a->word_count - 1) * BIGINT_WORD_BITS;
    bigint_word_t top_word = a->words[a->word_count - 1];

    // Count bits in top word
    while (top_word > 0)
    {
        bits++;
        top_word >>= 1;
    }

    return bits;
}

// Simplified stubs for modular arithmetic and other functions
int bigint_mod_add(bigint_t *result, const bigint_t *a, const bigint_t *b, const bigint_t *m)
{
    // Simplified: just add and assume result < m for now
    return bigint_add(result, a, b);
}

int bigint_mod_sub(bigint_t *result, const bigint_t *a, const bigint_t *b, const bigint_t *m)
{
    return bigint_sub(result, a, b);
}

int bigint_mod_mult(bigint_t *result, const bigint_t *a, const bigint_t *b, const bigint_t *m)
{
    return bigint_mult(result, a, b);
}

int bigint_mod_pow(bigint_t *result, const bigint_t *base, const bigint_t *exp, const bigint_t *m)
{
    // Stub implementation
    return bigint_copy(result, base), 0;
}

int bigint_mod(bigint_t *result, const bigint_t *a, const bigint_t *m)
{
    return bigint_copy(result, a), 0;
}

int bigint_shift_left(bigint_t *result, const bigint_t *a, uint32_t shift)
{
    // Simplified stub
    return bigint_copy(result, a), 0;
}

int bigint_shift_right(bigint_t *result, const bigint_t *a, uint32_t shift)
{
    // Simplified stub
    return bigint_copy(result, a), 0;
}

int bigint_get_bit(const bigint_t *a, uint32_t bit_pos)
{
    if (!a)
        return 0;

    uint32_t word_idx = bit_pos / BIGINT_WORD_BITS;
    uint32_t bit_idx = bit_pos % BIGINT_WORD_BITS;

    if (word_idx >= a->word_count)
        return 0;

    return (a->words[word_idx] >> bit_idx) & 1;
}

void bigint_set_bit(bigint_t *a, uint32_t bit_pos, int value)
{
    if (!a)
        return;

    uint32_t word_idx = bit_pos / BIGINT_WORD_BITS;
    uint32_t bit_idx = bit_pos % BIGINT_WORD_BITS;

    if (word_idx >= BIGINT_MAX_WORDS)
        return;

    if (word_idx >= a->word_count)
    {
        // Extend word count if needed
        for (uint32_t i = a->word_count; i <= word_idx; i++)
        {
            a->words[i] = 0;
        }
        a->word_count = word_idx + 1;
    }

    if (value)
    {
        a->words[word_idx] |= (1U << bit_idx);
    }
    else
    {
        a->words[word_idx] &= ~(1U << bit_idx);
    }
}

int bigint_random(bigint_t *result, uint32_t bit_length)
{
    // Simplified stub using math_hal
    if (!result)
        return -1;

    bigint_init(result);

    uint32_t words_needed = (bit_length + BIGINT_WORD_BITS - 1) / BIGINT_WORD_BITS;
    if (words_needed > BIGINT_MAX_WORDS)
        return -1;

    for (uint32_t i = 0; i < words_needed; i++)
    {
        uint8_t random_bytes[sizeof(bigint_word_t)];
        if (math_hal_rng_bytes(random_bytes, sizeof(random_bytes)) != 0)
        {
            return -1;
        }

        memcpy(&result->words[i], random_bytes, sizeof(bigint_word_t));
    }

    result->word_count = words_needed;

    // Mask off extra bits in the top word
    if (bit_length % BIGINT_WORD_BITS != 0)
    {
        uint32_t extra_bits = bit_length % BIGINT_WORD_BITS;
        bigint_word_t mask = (1U << extra_bits) - 1;
        result->words[words_needed - 1] &= mask;
    }

    bigint_normalize(result);
    return 0;
}

int bigint_random_mod(bigint_t *result, const bigint_t *max)
{
    // Simplified stub
    return bigint_random(result, 32);
}