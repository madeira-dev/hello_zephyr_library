#include "core/math/math_hal.h"
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>

// #ifdef CONFIG_SOC_NRF52840
// #include <hal/nrf_rng.h>
// #include <nrfx_rng.h>
// #endif

LOG_MODULE_REGISTER(math_hal, LOG_LEVEL_DBG);

// ============================================================================
// Modular Arithmetic Primitives
// ============================================================================

math_word_t math_hal_mod_add(math_word_t a, math_word_t b, math_word_t m)
{
    if (m == 0)
        return 0;

    // Prevent overflow in addition
    if (a >= m)
        a = a % m;
    if (b >= m)
        b = b % m;

    // #ifdef CONFIG_SOC_NRF52840
    //     uint64_t sum = (uint64_t)a + b;
    //     return (math_word_t)(sum % m);
    // #else
    // For larger word sizes, more complex overflow handling needed
    math_word_t sum = a + b;
    if (sum < a)
    { // Overflow occurred
        // Handle overflow case
        return (sum % m);
    }
    return sum >= m ? sum - m : sum;
    // #endif
}

math_word_t math_hal_mod_sub(math_word_t a, math_word_t b, math_word_t m)
{
    if (m == 0)
        return 0;

    // Ensure inputs are reduced
    a = a % m;
    b = b % m;

    if (a >= b)
    {
        return a - b;
    }
    else
    {
        return m - (b - a);
    }
}

math_word_t math_hal_mod_mult(math_word_t a, math_word_t b, math_word_t m)
{
    if (m == 0)
        return 0;

    // Reduce inputs
    a = a % m;
    b = b % m;

    // #ifdef CONFIG_SOC_NRF52840
    //     // Use 64-bit arithmetic for 32-bit words
    //     uint64_t prod = (uint64_t)a * b;
    //     return (math_word_t)(prod % m);
    // #else
    // For 64-bit words, we'd need 128-bit arithmetic
    // Simplified implementation using library functions
    return (a * b) % m;
    // #endif
}

math_word_t math_hal_mod_pow(math_word_t base, math_word_t exp, math_word_t m)
{
    if (m == 1)
        return 0;
    if (exp == 0)
        return 1;

    math_word_t result = 1;
    base = base % m;

    while (exp > 0)
    {
        if (exp & 1)
        {
            result = math_hal_mod_mult(result, base, m);
        }
        exp >>= 1;
        base = math_hal_mod_mult(base, base, m);
    }

    return result;
}

math_word_t math_hal_mod_inv(math_word_t a, math_word_t m)
{
    // Extended Euclidean Algorithm (simplified)
    if (m == 1)
        return 0;

    math_word_t m0 = m;
    math_word_t x0 = 0, x1 = 1;

    if (a == 1)
        return 1;

    while (a > 1)
    {
        math_word_t q = a / m;
        math_word_t t = m;

        m = a % m;
        a = t;
        t = x0;

        x0 = x1 - q * x0;
        x1 = t;
    }

    if (x1 < 0)
        x1 += m0;

    return x1;
}

// ============================================================================
// NTT Operations (Stubs)
// ============================================================================

int math_hal_ntt_init_params(ntt_params_t *params, uint32_t n, math_word_t modulus)
{
    if (!params || (n & (n - 1)) != 0)
        return -1; // n must be power of 2

    params->n = n;
    params->modulus = modulus;
    params->log_n = 0;

    // Calculate log2(n)
    uint32_t temp = n;
    while (temp > 1)
    {
        temp >>= 1;
        params->log_n++;
    }

    // Find primitive n-th root of unity (simplified)
    // In practice, this requires finding a generator and computing the right power
    params->root_of_unity = 3; // Placeholder
    params->inv_root_of_unity = math_hal_mod_inv(params->root_of_unity, modulus);
    params->inv_n = math_hal_mod_inv(n, modulus);

    LOG_DBG("Initialized NTT params: n=%u, modulus=%u", n, (uint32_t)modulus);
    return 0;
}

int math_hal_ntt_forward(math_word_t *data, const ntt_params_t *params)
{
    if (!data || !params)
        return -1;

    // Stub implementation of Cooley-Tukey NTT
    // In practice, this would be a complex algorithm

    LOG_DBG("Performed forward NTT (stub)");
    return 0;
}

int math_hal_ntt_inverse(math_word_t *data, const ntt_params_t *params)
{
    if (!data || !params)
        return -1;

    // Stub implementation
    LOG_DBG("Performed inverse NTT (stub)");
    return 0;
}

int math_hal_ntt_mult(math_word_t *result, const math_word_t *a,
                      const math_word_t *b, const ntt_params_t *params)
{
    if (!result || !a || !b || !params)
        return -1;

    // Element-wise multiplication in NTT domain
    for (uint32_t i = 0; i < params->n; i++)
    {
        result[i] = math_hal_mod_mult(a[i], b[i], params->modulus);
    }

    LOG_DBG("Performed NTT domain multiplication");
    return 0;
}

// ============================================================================
// Random Number Generation
// ============================================================================

static bool rng_initialized = false;

int math_hal_rng_init(void)
{
    LOG_INF("Using Zephyr software RNG");
    rng_initialized = true;

    return 0;
}

int math_hal_rng_bytes(uint8_t *buffer, size_t size)
{
    if (!buffer || size == 0)
        return -1;

    if (!rng_initialized)
    {
        if (math_hal_rng_init() != 0)
            return -1;
    }

    sys_rand_get(buffer, size);
    return 0;
}

// #ifdef CONFIG_SOC_NRF52840
// int math_hal_hw_rng_bytes(uint8_t *buffer, size_t size)
// {
//     if (!buffer || size == 0)
//         return -1;

//     for (size_t i = 0; i < size; i++)
//     {
//         // Wait for random value to be ready
//         while (!nrf_rng_event_check(NRF_RNG, NRF_RNG_EVENT_VALRDY))
//         {
//             k_yield(); // Allow other threads to run
//         }

//         // Read random byte
//         buffer[i] = nrf_rng_random_value_get(NRF_RNG);

//         // Clear event
//         nrf_rng_event_clear(NRF_RNG, NRF_RNG_EVENT_VALRDY);
//     }

//     return 0;
// }
// #endif

math_word_t math_hal_rng_uniform(math_word_t max)
{
    if (max == 0)
        return 0;

    math_word_t result;
    int ret = math_hal_rng_bytes((uint8_t *)&result, sizeof(result));
    if (ret != 0)
        return 0;

    return result % max;
}

math_word_t math_hal_rng_mod(math_word_t m)
{
    return math_hal_rng_uniform(m);
}

// ============================================================================
// Memory and Performance Utilities
// ============================================================================

void math_hal_secure_zero(void *ptr, size_t size)
{
    if (!ptr || size == 0)
        return;

    volatile uint8_t *p = (volatile uint8_t *)ptr;
    for (size_t i = 0; i < size; i++)
    {
        p[i] = 0;
    }
}

bool math_hal_is_prime(math_word_t n)
{
    if (n < 2)
        return false;
    if (n == 2)
        return true;
    if (n % 2 == 0)
        return false;

    // Simple trial division up to sqrt(n)
    for (math_word_t i = 3; i * i <= n; i += 2)
    {
        if (n % i == 0)
            return false;
    }

    return true;
}

math_word_t math_hal_next_prime(math_word_t n)
{
    if (n < 2)
        return 2;

    // Make odd if even
    if (n % 2 == 0)
        n++;

    while (!math_hal_is_prime(n))
    {
        n += 2;
    }

    return n;
}

math_word_t math_hal_gcd(math_word_t a, math_word_t b)
{
    while (b != 0)
    {
        math_word_t temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

uint64_t math_hal_get_cycles(void)
{
    // #ifdef CONFIG_SOC_NRF52840
    //     // Use ARM DWT cycle counter if available
    //     return k_cycle_get_64();
    // #else
    return k_uptime_get();
    // #endif
}