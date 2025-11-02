#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "openfhe_embedded.h"
#include "internal/ckks_params.h"
#include "internal/ntt.h"

// Opaque plan struct definition
struct oe_ntt_plan_s
{
  uint32_t ring_dim;
  uint32_t num_q;
  oe_ntt_mode_t mode;
  uint32_t *fwd_roots[OE_MAX_LIMBS];
  uint32_t *inv_roots[OE_MAX_LIMBS];
};

static bool is_power_of_two(uint32_t n)
{
  return n && !(n & (n - 1));
}

static inline uint32_t mul_mod_u32(uint32_t a, uint32_t b, uint32_t q)
{
  return (uint32_t)(((uint64_t)a * (uint64_t)b) % (uint64_t)q);
}

static uint32_t pow_mod_u32(uint32_t base, uint32_t exp, uint32_t q)
{
  if (q == 0)
  {
    return 0;
  }
  base %= q;
  uint32_t result = 1 % q;
  while (exp)
  {
    if (exp & 1)
    {
      result = mul_mod_u32(result, base, q);
    }
    base = mul_mod_u32(base, base, q);
    exp >>= 1;
  }
  return result;
}

static uint32_t find_primitive_root_2N(uint32_t q, uint32_t N)
{
  if (q < 3 || N == 0 || !is_power_of_two(N) || ((q - 1) % (2 * N)) != 0)
  {
    return 0;
  }
  uint32_t order = 2 * N;
  uint32_t e = (q - 1) / order;
  for (uint32_t g = 2; g <= q - 2; g++)
  {
    uint32_t omega = pow_mod_u32(g, e, q);
    if (omega <= 1)
      continue;
    if (pow_mod_u32(omega, order, q) == 1 && pow_mod_u32(omega, N, q) == q - 1)
    {
      return omega;
    }
  }
  return 0;
}

static void build_twiddles_forward(uint32_t q, uint32_t N, uint32_t omega, uint32_t *out_table, size_t len)
{
  if (q < 3 || N == 0 || !is_power_of_two(N) || omega == 0 || out_table == NULL || len < N)
  {
    return;
  }
  out_table[0] = 1;
  for (uint32_t i = 1; i < N; i++)
  {
    out_table[i] = mul_mod_u32(out_table[i - 1], omega, q);
  }
}

static void build_twiddles_inverse(uint32_t q, uint32_t N, uint32_t omega_inv, uint32_t *out_table, size_t len)
{
  if (q < 3 || N == 0 || !is_power_of_two(N) || omega_inv == 0 || out_table == NULL || len < N)
  {
    return;
  }
  out_table[0] = 1;
  for (uint32_t i = 1; i < N; i++)
  {
    out_table[i] = mul_mod_u32(out_table[i - 1], omega_inv, q);
  }
}

static bool carve_per_prime_tables_from_pool(void *pool, size_t pool_bytes, uint32_t ring_dim, uint32_t num_q, oe_ntt_mode_t mode, uint32_t **out_fwd_bases, uint32_t **out_inv_bases, size_t *io_consumed_bytes)
{
  if (mode == OE_NTT_ON_THE_FLY)
  {
    if (out_fwd_bases && out_inv_bases)
    {
      for (uint32_t i = 0; i < num_q; ++i)
      {
        out_fwd_bases[i] = NULL;
        out_inv_bases[i] = NULL;
      }
    }
    if (io_consumed_bytes)
      *io_consumed_bytes = 0;
    return true;
  }

  // mode == OE_NTT_PRECOMPUTED
  if (!pool || !out_fwd_bases || !out_inv_bases || !io_consumed_bytes)
    return false;
  if (num_q == 0)
    return false;
  if (!is_power_of_two(ring_dim))
    return false;

  // Align pool pointer to 4 bytes
  uintptr_t pool_addr = (uintptr_t)pool;
  uintptr_t aligned_addr = (pool_addr + 3) & ~(uintptr_t)3;
  size_t align_offset = aligned_addr - pool_addr;
  if (pool_bytes < align_offset)
    return false;
  uint8_t *cursor = (uint8_t *)aligned_addr;
  size_t avail_bytes = pool_bytes - align_offset;

  // Compute per-prime table size and check for overflow
  if (ring_dim > SIZE_MAX / 8)
    return false;
  size_t per_prime = 2 * ring_dim * sizeof(uint32_t);
  if (num_q > SIZE_MAX / per_prime)
    return false;
  size_t total_bytes = num_q * per_prime;
  if (avail_bytes < total_bytes)
    return false;

  for (uint32_t i = 0; i < num_q; ++i)
  {
    out_fwd_bases[i] = (uint32_t *)cursor;
    out_inv_bases[i] = (uint32_t *)(cursor + ring_dim * sizeof(uint32_t));
    cursor += per_prime;
  }
  *io_consumed_bytes = (size_t)(cursor - (uint8_t *)pool);
  return true;
}

static inline uint32_t twiddle(const uint32_t *roots, uint32_t q, uint32_t N, uint32_t e)
{
  uint32_t idx2 = e << 1;
  if (idx2 < N)
    return roots[idx2];
  else
    return q - roots[idx2 - N];
}

oe_status_t ntt_plan_requirements(uint32_t ring_dim, uint32_t num_q, const uint32_t *q, oe_ntt_mode_t mode, size_t *out_size_hint)
{
  if (!out_size_hint)
    return OE_ERR_INVALID_ARG;

  if (mode == OE_NTT_ON_THE_FLY)
  {
    *out_size_hint = 0;
    return OE_OK;
  }
  else if (mode == OE_NTT_PRECOMPUTED)
  {
    if (ring_dim == 0 || !is_power_of_two(ring_dim) || num_q == 0 || q == NULL)
      return OE_ERR_INVALID_ARG;
    // Check for overflow: ring_dim > SIZE_MAX / 8
    if (ring_dim > SIZE_MAX / 8)
      return OE_ERR_INVALID_ARG;
    size_t per_prime = 2 * ring_dim * sizeof(uint32_t);
    // Check for overflow: num_q > (SIZE_MAX - 3) / per_prime
    if (per_prime == 0 || num_q > (SIZE_MAX - 3) / per_prime)
      return OE_ERR_INVALID_ARG;
    size_t total = num_q * per_prime + 3;
    *out_size_hint = total;
    return OE_OK;
  }
  else
  {
    return OE_ERR_INVALID_ARG;
  }
}

size_t ntt_plan_exact_bytes(uint32_t ring_dim, uint32_t num_q, oe_ntt_mode_t mode)
{
  if (mode == OE_NTT_ON_THE_FLY)
  {
    return 0;
  }
  else if (mode == OE_NTT_PRECOMPUTED)
  {
    if (ring_dim == 0 || !is_power_of_two(ring_dim) || num_q == 0)
      return 0;
    if (ring_dim > SIZE_MAX / 8)
      return 0;
    size_t per_prime = 2 * ring_dim * sizeof(uint32_t);
    if (per_prime == 0 || num_q > (SIZE_MAX - 3) / per_prime)
      return 0;
    return num_q * per_prime + 3;
  }
  else
  {
    return 0;
  }
}

oe_status_t ntt_plan_init(uint32_t ring_dim,
                          uint32_t num_q,
                          const uint32_t *q,
                          const oe_derived_consts_t *dc,
                          oe_ntt_mode_t mode,
                          void *mempool,
                          size_t mempool_bytes,
                          oe_ntt_plan_t **out_plan)
{
  if (!out_plan)
    return OE_ERR_INVALID_ARG;
  *out_plan = NULL;

  // Basic input checks
  if (ring_dim == 0 || !is_power_of_two(ring_dim) || num_q == 0 || !q || !dc)
    return OE_ERR_INVALID_ARG;
  if (mode != OE_NTT_ON_THE_FLY && mode != OE_NTT_PRECOMPUTED)
    return OE_ERR_INVALID_ARG;

  // Allocate a small plan object (tables live in mempool, not the heap)
  struct oe_ntt_plan_s *plan = (struct oe_ntt_plan_s *)malloc(sizeof(struct oe_ntt_plan_s));
  if (!plan)
    return OE_ERR_INVALID_ARG; // using INVALID_ARG since NO_MEMORY is not part of your codeset
  plan->ring_dim = ring_dim;
  plan->num_q = num_q;
  plan->mode = mode;
  for (uint32_t i = 0; i < OE_MAX_LIMBS; ++i)
  {
    plan->fwd_roots[i] = NULL;
    plan->inv_roots[i] = NULL;
  }

  // Carve or null-out per-prime table pointers from caller's mempool
  size_t consumed_bytes = 0;
  if (!carve_per_prime_tables_from_pool(mempool, mempool_bytes,
                                        ring_dim, num_q, mode,
                                        plan->fwd_roots, plan->inv_roots,
                                        &consumed_bytes))
  {
    free(plan);
    return OE_ERR_INVALID_ARG;
  }

  // In ON_THE_FLY mode there are no precomputed tables to build
  if (mode == OE_NTT_ON_THE_FLY)
  {
    *out_plan = plan;
    return OE_OK;
  }

  // PRECOMPUTED: generate twiddle tables per prime
  for (uint32_t j = 0; j < num_q; ++j)
  {
    const uint32_t qj = q[j];

    // Derive primitive 2N-th root (omega) and its inverse in F_qj
    uint32_t omega = find_primitive_root_2N(qj, ring_dim);
    if (omega == 0)
    {
      free(plan);
      return OE_ERR_INVALID_ARG;
    }
    uint32_t omega_inv = pow_mod_u32(omega, qj - 2, qj); // Fermat inverse (qj is prime)

    uint32_t *fwd_table = plan->fwd_roots[j];
    uint32_t *inv_table = plan->inv_roots[j];
    if (!fwd_table || !inv_table)
    {
      free(plan);
      return OE_ERR_INVALID_ARG;
    }

    // Build contiguous twiddle tables of length N
    build_twiddles_forward(qj, ring_dim, omega, fwd_table, ring_dim);
    build_twiddles_inverse(qj, ring_dim, omega_inv, inv_table, ring_dim);
  }

  *out_plan = plan;
  return OE_OK;
}

void ntt_plan_free(oe_ntt_plan_t *plan)
{
  if (plan == NULL)
    return;
  // Do not free or modify caller-owned mempool buffers.
  free(plan);
}

void ntt_forward_inplace(uint32_t *a, uint32_t q, const oe_ntt_plan_t *plan, uint32_t prime_index)
{
  if (!a || !plan || prime_index >= plan->num_q || q < 3 || plan->ring_dim == 0 || !is_power_of_two(plan->ring_dim))
  {
    return;
  }
  uint32_t N = plan->ring_dim;
  if (plan->mode == OE_NTT_PRECOMPUTED)
  {
    const uint32_t *roots = plan->fwd_roots[prime_index];
    if (!roots)
      return;
    // Lambda-like static inline for ω_N^e from 2N-root table
    // roots[0..N-1] = powers of ω, roots[N..2N-1] = not used (table is size N, see build_twiddles_forward)
    // But we want to fetch ω_N^e for e in [0, N-1], using the 2N-th root table:
    // Actually, in table, roots[0..N-1] = ω^{0..N-1}, but for twiddle indices, we want to simulate ω_N^e for e in [0,N-1], and for e >= N, ω_N^e = -ω_N^{e-N}.
    // Here, we use the 2N-th root trick (from Bluestein/NTT literature): for index e, fetch roots[2e] if 2e < N, else q - roots[2e-N].
    for (uint32_t m = 2; m <= N; m <<= 1)
    {
      uint32_t step_e = N / m;
      for (uint32_t k = 0; k < N; k += m)
      {
        uint32_t w = 1;
        uint32_t w_step = twiddle(roots, q, N, step_e);
        for (uint32_t j = 0; j < m / 2; ++j)
        {
          uint32_t u = a[k + j];
          uint32_t v = a[k + j + m / 2];
          uint64_t t = (uint64_t)w * v % q;
          uint32_t sum = u + t;
          a[k + j] = (sum >= q) ? (sum - q) : sum;
          a[k + j + m / 2] = (u >= t) ? (u - t) : (u + q - t);
          w = (uint32_t)((uint64_t)w * w_step % q);
        }
      }
    }
  }
  else
  {
    // ON_THE_FLY
    uint32_t omega = find_primitive_root_2N(q, N);
    if (omega == 0)
      return;
    uint32_t omega2 = mul_mod_u32(omega, omega, q); // primitive N-th root
    for (uint32_t m = 2; m <= N; m <<= 1)
    {
      uint32_t w_step = pow_mod_u32(omega2, N / m, q);
      for (uint32_t k = 0; k < N; k += m)
      {
        uint32_t w = 1;
        for (uint32_t j = 0; j < m / 2; ++j)
        {
          uint32_t u = a[k + j];
          uint32_t v = a[k + j + m / 2];
          uint64_t t = (uint64_t)w * v % q;
          uint32_t sum = u + t;
          a[k + j] = (sum >= q) ? (sum - q) : sum;
          a[k + j + m / 2] = (u >= t) ? (u - t) : (u + q - t);
          w = (uint32_t)((uint64_t)w * w_step % q);
        }
      }
    }
  }
}

void ntt_inverse_inplace(uint32_t *a,
                         uint32_t q,
                         uint32_t n_inv_mod_q,
                         const oe_ntt_plan_t *plan,
                         uint32_t prime_index)
{
  if (!a || !plan || prime_index >= plan->num_q || q < 3 || plan->ring_dim == 0 || !is_power_of_two(plan->ring_dim))
    return;

  const uint32_t N = plan->ring_dim;

  if (plan->mode == OE_NTT_PRECOMPUTED)
  {
    const uint32_t *roots = plan->inv_roots[prime_index];
    if (!roots)
      return;

    // Gentleman–Sande (DIF) inverse
    for (uint32_t m = N; m >= 2; m >>= 1)
    {
      uint32_t step_e = N / m; // exponent stride for ω_N^{-1}
      for (uint32_t k = 0; k < N; k += m)
      {
        uint32_t w = 1;
        uint32_t w_step = twiddle(roots, q, N, step_e); // ω_N^{-step_e}
        for (uint32_t j = 0; j < (m >> 1); ++j)
        {
          uint32_t u = a[k + j];
          uint32_t v = a[k + j + (m >> 1)];
          uint32_t sum = u + v; // top branch: u + v
          if (sum >= q)
            sum -= q;
          uint32_t diff = (u >= v) ? (u - v) : (u + q - v); // bottom diff
          uint32_t t = (uint32_t)(((uint64_t)w * diff) % q);
          a[k + j] = sum;
          a[k + j + (m >> 1)] = t;
          w = (uint32_t)(((uint64_t)w * w_step) % q);
        }
      }
    }

    // Final scale by N^{-1} mod q
    for (uint32_t i = 0; i < N; ++i)
    {
      a[i] = (uint32_t)(((uint64_t)a[i] * n_inv_mod_q) % q);
    }
  }
  else
  {
    // ON_THE_FLY: derive ω (2N-th primitive root) and use ω_N = ω^2, then ω_N^{-1}
    uint32_t omega = find_primitive_root_2N(q, N);
    if (omega == 0)
      return;
    uint32_t omega2 = mul_mod_u32(omega, omega, q);      // primitive N-th root
    uint32_t omega2_inv = pow_mod_u32(omega2, q - 2, q); // inverse of ω_N

    for (uint32_t m = N; m >= 2; m >>= 1)
    {
      uint32_t w_step = pow_mod_u32(omega2_inv, N / m, q); // stage twiddle
      for (uint32_t k = 0; k < N; k += m)
      {
        uint32_t w = 1;
        for (uint32_t j = 0; j < (m >> 1); ++j)
        {
          uint32_t u = a[k + j];
          uint32_t v = a[k + j + (m >> 1)];
          uint32_t sum = u + v;
          if (sum >= q)
            sum -= q;
          uint32_t diff = (u >= v) ? (u - v) : (u + q - v);
          uint32_t t = (uint32_t)(((uint64_t)w * diff) % q);
          a[k + j] = sum;
          a[k + j + (m >> 1)] = t;
          w = (uint32_t)(((uint64_t)w * w_step) % q);
        }
      }
    }

    // Final scale by N^{-1} mod q
    for (uint32_t i = 0; i < N; ++i)
    {
      a[i] = (uint32_t)(((uint64_t)a[i] * n_inv_mod_q) % q);
    }
  }
}

const uint32_t *ntt_plan_get_fwd_roots(const oe_ntt_plan_t *plan,
                                       uint32_t prime_index,
                                       size_t *out_len)
{
  if (out_len)
    *out_len = 0;
  if (!plan)
    return NULL;
  if (prime_index >= plan->num_q)
    return NULL;

  // Only meaningful in PRECOMPUTED mode; ON_THE_FLY has no stored tables.
  if (plan->mode != OE_NTT_PRECOMPUTED)
    return NULL;

  const uint32_t *tbl = plan->fwd_roots[prime_index];
  if (!tbl)
    return NULL;

  if (out_len)
    *out_len = plan->ring_dim; // contiguous table of length N
  return tbl;
}

const uint32_t *ntt_plan_get_inv_roots(const oe_ntt_plan_t *plan,
                                       uint32_t prime_index,
                                       size_t *out_len)
{
  if (out_len)
    *out_len = 0;
  if (!plan)
    return NULL;
  if (prime_index >= plan->num_q)
    return NULL;

  // Only meaningful in PRECOMPUTED mode; ON_THE_FLY has no stored tables.
  if (plan->mode != OE_NTT_PRECOMPUTED)
    return NULL;

  const uint32_t *tbl = plan->inv_roots[prime_index];
  if (!tbl)
    return NULL;

  if (out_len)
    *out_len = plan->ring_dim; // contiguous table of length N
  return tbl;
}

uint32_t ntt_plan_ring_dim(const oe_ntt_plan_t *plan)
{
  if (!plan)
    return 0;
  return plan->ring_dim;
}

oe_ntt_mode_t ntt_plan_mode(const oe_ntt_plan_t *plan)
{
  if (!plan)
    return OE_NTT_ON_THE_FLY; // defensive default (no precomputed tables)
  return plan->mode;
}
