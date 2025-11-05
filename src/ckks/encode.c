#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

#include "openfhe_embedded.h"
#include "internal/encode.h"

struct oe_encode_plan_s
{
  uint32_t ring_dim;
  oe_ifft_mode_t mode;
  double *twiddles; /* length = 2 * ring_dim doubles: [Re0, Im0, Re1, Im1, ...] */
  uint16_t *bitrev; /* length = ring_dim entries */
};

/*
 * Return a size hint (bytes) for an encoding/IFFT plan under the given mode.
 * Sizing model mirrors ckks_params_estimate_ifft_plan_bytes():
 *   PRECOMPUTED: ring_dim * 18 + 32
 *   ON_THE_FLY: 0
 */
oe_status_t encode_plan_requirements(uint32_t ring_dim,
                                     oe_ifft_mode_t mode,
                                     size_t *out_size_hint)
{
  if (out_size_hint == NULL)
  {
    return OE_ERR_INVALID_ARG;
  }

  if (mode == OE_IFFT_ON_THE_FLY)
  {
    *out_size_hint = 0;
    return OE_OK;
  }
  else if (mode == OE_IFFT_PRECOMPUTED)
  {
    /* Validate ring_dim: non-zero power of two */
    if (ring_dim == 0 || (ring_dim & (ring_dim - 1)) != 0)
    {
      return OE_ERR_INVALID_ARG;
    }

    /* Overflow guard: ring_dim * 18 + 32 must fit in size_t */
    if (ring_dim > (SIZE_MAX - (size_t)32) / (size_t)18)
    {
      return OE_ERR_INVALID_ARG;
    }

    *out_size_hint = (size_t)ring_dim * (size_t)18 + (size_t)32;
    return OE_OK;
  }

  return OE_ERR_INVALID_ARG;
}

size_t encode_plan_exact_bytes(uint32_t ring_dim, oe_ifft_mode_t mode)
{
  if (mode == OE_IFFT_ON_THE_FLY)
  {
    return 0;
  }
  else if (mode == OE_IFFT_PRECOMPUTED)
  {
    /* Validate ring_dim: non-zero power of two */
    if (ring_dim == 0 || (ring_dim & (ring_dim - 1)) != 0)
    {
      return 0;
    }

    /* Overflow guard: ring_dim * 18 + 32 must fit in size_t */
    if (ring_dim > (SIZE_MAX - (size_t)32) / (size_t)18)
    {
      return 0;
    }

    return (size_t)ring_dim * (size_t)18 + (size_t)32;
  }

  return 0; /* invalid mode */
}

static bool is_power_of_two(uint32_t n)
{
  return n && !(n & (n - 1));
}

static bool carve_from_pool(void *pool,
                            size_t pool_bytes,
                            uint32_t ring_dim,
                            double **out_tw,
                            uint16_t **out_perm,
                            size_t *out_consumed)
{
  /* Basic argument checks */
  if (!pool || pool_bytes == 0 || ring_dim == 0 || !out_tw || !out_perm || !out_consumed)
  {
    return false;
  }

  /* Initialize outputs defensively */
  *out_tw = NULL;
  *out_perm = NULL;
  *out_consumed = 0;

  /* Align to 8-byte boundary for double */
  uintptr_t base = (uintptr_t)pool;
  uintptr_t aligned = (base + (uintptr_t)7) & ~((uintptr_t)7);
  size_t align_offset = (size_t)(aligned - base);
  if (align_offset > pool_bytes)
  {
    return false;
  }

  uint8_t *cursor = (uint8_t *)aligned;
  size_t avail = pool_bytes - align_offset;

  /* Compute sizes with overflow guards */
  /* twiddles: 2 * ring_dim doubles */
  const size_t dbl_sz = sizeof(double);
  const size_t u16_sz = sizeof(uint16_t);

  if (ring_dim > SIZE_MAX / 2u)
  {
    return false; /* 2*ring_dim overflow in size_t domain */
  }
  size_t tw_elems = (size_t)2u * (size_t)ring_dim;

  if (dbl_sz != 8)
  {
    /* On every toolchain we target double is 8 bytes; keep a guard anyway */
    if (tw_elems > SIZE_MAX / dbl_sz)
    {
      return false;
    }
  }
  size_t tw_bytes = tw_elems * dbl_sz; /* equals 16 * ring_dim when dbl==8 */

  if (tw_bytes > avail)
  {
    return false;
  }

  double *twiddles = (double *)cursor;
  cursor += tw_bytes;
  avail -= tw_bytes;

  /* permutation: ring_dim * uint16_t */
  if (u16_sz != 2)
  {
    if ((size_t)ring_dim > SIZE_MAX / u16_sz)
    {
      return false;
    }
  }
  size_t perm_bytes = (size_t)ring_dim * u16_sz;
  if (perm_bytes > avail)
  {
    return false;
  }

  uint16_t *perm = (uint16_t *)cursor;
  cursor += perm_bytes;

  /* Compute total consumed relative to original pool base */
  size_t total_consumed = align_offset + tw_bytes + perm_bytes;
  if (total_consumed > pool_bytes)
  {
    return false; /* Should not happen given checks, but keep defensive */
  }

  *out_tw = twiddles;
  *out_perm = perm;
  *out_consumed = total_consumed;
  return true;
}

static void build_twiddles(uint32_t N, double *tw)
{
  if (N == 0 || tw == NULL)
  {
    return;
  }
  /* Use a local definition of PI to avoid reliance on non-standard macros. */
  const double PI = 3.141592653589793238462643383279502884;
  const double step = -2.0 * PI / (double)N; /* negative for IFFT twiddles */

  for (uint32_t k = 0; k < N; ++k)
  {
    const double angle = step * (double)k;
    /* Layout: [Re0, Im0, Re1, Im1, ...] */
    tw[2u * k] = cos(angle);
    tw[2u * k + 1] = sin(angle);
  }
}

static void build_bitrev(uint32_t N, uint16_t *perm)
{
  if (N == 0 || perm == NULL)
  {
    return;
  }

  /* Compute the number of bits needed to represent indices [0, N-1] */
  uint32_t bits = 0;
  uint32_t temp = N;
  while (temp > 1)
  {
    bits++;
    temp >>= 1;
  }

  for (uint32_t i = 0; i < N; ++i)
  {
    uint32_t x = i;
    uint32_t r = 0;
    for (uint32_t b = 0; b < bits; ++b)
    {
      r = (r << 1) | (x & 1u);
      x >>= 1u;
    }
    perm[i] = (uint16_t)r;
  }
}

oe_status_t encode_plan_init(uint32_t ring_dim,
                             oe_ifft_mode_t mode,
                             void *mempool,
                             size_t mempool_bytes,
                             oe_encode_plan_t **out_plan)
{
  if (out_plan == NULL)
  {
    return OE_ERR_INVALID_ARG;
  }
  *out_plan = NULL;

  /* Validate mode */
  if (!(mode == OE_IFFT_ON_THE_FLY || mode == OE_IFFT_PRECOMPUTED))
  {
    return OE_ERR_INVALID_ARG;
  }

  /* Allocate the small plan object (caller owns mempool) */
  oe_encode_plan_t *plan = (oe_encode_plan_t *)malloc(sizeof(*plan));
  if (!plan)
  {
    return OE_ERR_INVALID_ARG; /* conservative error in absence of OE_ERR_NO_MEMORY */
  }
  plan->ring_dim = ring_dim;
  plan->mode = mode;
  plan->twiddles = NULL;
  plan->bitrev = NULL;

  if (mode == OE_IFFT_ON_THE_FLY)
  {
    /* Metadata-only plan; ring_dim may be 0 (unused) */
    *out_plan = plan;
    return OE_OK;
  }

  /* PRECOMPUTED path: validate inputs */
  if (ring_dim == 0 || !is_power_of_two(ring_dim))
  {
    free(plan);
    return OE_ERR_INVALID_ARG;
  }
  if (mempool == NULL || mempool_bytes == 0)
  {
    free(plan);
    return OE_ERR_INVALID_ARG;
  }

  /* Carve aligned slices for twiddles and permutation from caller's pool */
  double *tw = NULL;
  uint16_t *perm = NULL;
  size_t consumed = 0;
  if (!carve_from_pool(mempool, mempool_bytes, ring_dim, &tw, &perm, &consumed))
  {
    free(plan);
    return OE_ERR_INVALID_ARG;
  }

  /* Build precomputed data */
  build_twiddles(ring_dim, tw);
  build_bitrev(ring_dim, perm);

  plan->twiddles = tw;
  plan->bitrev = perm;

  *out_plan = plan;
  return OE_OK;
}

void encode_plan_free(oe_encode_plan_t *plan)
{
  if (plan == NULL)
  {
    return;
  }
  /* twiddles/bitrev live in caller-owned mempool; do not free them here. */
  free(plan);
}

uint32_t encode_plan_ring_dim(const oe_encode_plan_t *plan)
{
  if (plan == NULL)
  {
    return 0u;
  }
  return plan->ring_dim;
}

oe_ifft_mode_t encode_plan_mode(const oe_encode_plan_t *plan)
{
  if (plan == NULL)
  {
    return OE_IFFT_ON_THE_FLY; /* defensive default */
  }
  return plan->mode;
}

const double *encode_plan_get_twiddles(const oe_encode_plan_t *plan, size_t *out_len)
{
  if (out_len)
  {
    *out_len = 0;
  }
  if (plan == NULL)
  {
    return NULL;
  }
  /* Twiddles are only materialized in PRECOMPUTED mode. */
  if (plan->mode != OE_IFFT_PRECOMPUTED)
  {
    return NULL;
  }
  if (plan->twiddles == NULL)
  {
    return NULL;
  }
  if (out_len)
  {
    /* Interleaved complex layout: [Re0, Im0, Re1, Im1, ...] => 2 * N doubles */
    *out_len = (size_t)2u * (size_t)plan->ring_dim;
  }
  return plan->twiddles;
}

const uint16_t *encode_plan_get_bitrev(const oe_encode_plan_t *plan, size_t *out_len)
{
  if (out_len)
  {
    *out_len = 0;
  }
  if (plan == NULL)
  {
    return NULL;
  }
  /* Bit-reversal permutation is only present in PRECOMPUTED mode. */
  if (plan->mode != OE_IFFT_PRECOMPUTED)
  {
    return NULL;
  }
  if (plan->bitrev == NULL)
  {
    return NULL;
  }
  if (out_len)
  {
    *out_len = (size_t)plan->ring_dim;
  }
  return plan->bitrev;
}
