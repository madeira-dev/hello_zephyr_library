#ifndef INTERNAL_ENCODE_H
#define INTERNAL_ENCODE_H

#include <stddef.h>
#include <stdint.h>

#include "openfhe_embedded.h"
#include "internal/ckks_params.h"

typedef struct oe_encode_plan_s oe_encode_plan_t;

#ifdef __cplusplus
extern "C"
{
#endif

  /* Return a size hint (bytes) for an encoding/IFFT plan under the given mode.
   * - OE_IFFT_ON_THE_FLY: *out_size_hint = 0 (no precomputed buffers).
   * - OE_IFFT_PRECOMPUTED: size for twiddle (complex) + permutation tables + small metadata.
   * Returns:
   *   OE_OK on success; OE_ERR_INVALID_ARG on bad inputs.
   */
  oe_status_t encode_plan_requirements(uint32_t ring_dim,
                                       oe_ifft_mode_t mode,
                                       size_t *out_size_hint);

  /* Return the exact number of bytes the encode plan will consume from the mempool.
   * Mirrors encode_plan_requirements, but without an out-param.
   * Returns 0 for invalid inputs or ON_THE_FLY mode.
   */
  size_t encode_plan_exact_bytes(uint32_t ring_dim,
                                 oe_ifft_mode_t mode);

  /* Initialize an encoding/IFFT plan.
   * - ON_THE_FLY: plan holds only metadata; mempool may be NULL/0.
   * - PRECOMPUTED: twiddle/permutation tables are carved from 'mempool'.
   * Ownership:
   *   The plan object (oe_encode_plan_t*) is heap-allocated and must be freed by encode_plan_free().
   *   The mempool is caller-owned and is NOT freed by encode_plan_free().
   */
  oe_status_t encode_plan_init(uint32_t ring_dim,
                               oe_ifft_mode_t mode,
                               void *mempool,
                               size_t mempool_bytes,
                               oe_encode_plan_t **out_plan);

  /* Free the plan object (does NOT free the caller's mempool). */
  void encode_plan_free(oe_encode_plan_t *plan);

  /* Accessors */
  uint32_t encode_plan_ring_dim(const oe_encode_plan_t *plan);
  oe_ifft_mode_t encode_plan_mode(const oe_encode_plan_t *plan);

  /* Twiddles and permutation accessors (PRECOMPUTED mode only).
   * - twiddles: contiguous array of length == ring_dim; stored as interleaved doubles [Re0, Im0, Re1, Im1, ...]
   *             or (if we choose real-only structure later) a documented layout. For now, tests only check presence/length.
   * - bitrev:   contiguous array of uint16_t (length == ring_dim) with the bit-reversal permutation.
   * On ON_THE_FLY plans, these return NULL and *out_len = 0.
   */
  const double *encode_plan_get_twiddles(const oe_encode_plan_t *plan, size_t *out_len);
  const uint16_t *encode_plan_get_bitrev(const oe_encode_plan_t *plan, size_t *out_len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* INTERNAL_ENCODE_H */
