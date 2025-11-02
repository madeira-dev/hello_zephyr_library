#ifndef NTT_INTERNAL_H
#define NTT_INTERNAL_H

/* NTT (Number-Theoretic Transform) internal API
 * ----------------------------------------------
 * Plan-based, per-prime NTT used by CKKS. Large buffers live in the caller's
 * mempool; the plan stores offsets/metadata. Supports ON-THE-FLY and
 * PRECOMPUTED modes.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "openfhe_embedded.h"     /* oe_ntt_mode_t */
#include "internal/ckks_params.h" /* oe_derived_consts_t */

#ifdef __cplusplus
extern "C"
{
#endif

  /* Opaque plan object (defined privately in ntt.c). */
  typedef struct oe_ntt_plan_s oe_ntt_plan_t;

  /* -------------------- sizing / requirements -------------------- */

  /* Conservative mempool hint; 0 for OE_NTT_ON_THE_FLY. */
  oe_status_t ntt_plan_requirements(uint32_t ring_dim,
                                    uint32_t num_q,
                                    const uint32_t *q,
                                    oe_ntt_mode_t mode,
                                    /*out*/ size_t *out_size_hint);

  /* Exact mempool bytes the plan will occupy; 0 for OE_NTT_ON_THE_FLY. */
  size_t ntt_plan_exact_bytes(uint32_t ring_dim,
                              uint32_t num_q,
                              oe_ntt_mode_t mode);

  /* ----------------------------- lifecycle ----------------------------- */

  oe_status_t ntt_plan_init(uint32_t ring_dim,
                            uint32_t num_q,
                            const uint32_t *q,
                            const oe_derived_consts_t *dc,
                            oe_ntt_mode_t mode,
                            void *mempool,
                            size_t mempool_bytes,
                            /*out*/ oe_ntt_plan_t **out_plan);

  void ntt_plan_free(oe_ntt_plan_t *plan);

  /* ----------------------------- transforms ----------------------------- */

  /* In-place forward NTT of length N modulo q; prime_index selects tables. */
  void ntt_forward_inplace(uint32_t *a,
                           uint32_t q,
                           const oe_ntt_plan_t *plan,
                           uint32_t prime_index);

  /* In-place inverse NTT of length N modulo q, including multiply by N^{-1} mod q. */
  void ntt_inverse_inplace(uint32_t *a,
                           uint32_t q,
                           uint32_t n_inv_mod_q,
                           const oe_ntt_plan_t *plan,
                           uint32_t prime_index);

  /* ----------------------------- accessors ----------------------------- */

  /* Return precomputed forward twiddles for a prime (or NULL in on-the-fly mode). */
  const uint32_t *ntt_plan_get_fwd_roots(const oe_ntt_plan_t *plan,
                                         uint32_t prime_index,
                                         size_t *out_len);

  /* Return precomputed inverse twiddles for a prime (or NULL in on-the-fly mode). */
  const uint32_t *ntt_plan_get_inv_roots(const oe_ntt_plan_t *plan,
                                         uint32_t prime_index,
                                         size_t *out_len);

  /* Metadata helpers. */
  uint32_t ntt_plan_ring_dim(const oe_ntt_plan_t *plan);
  oe_ntt_mode_t ntt_plan_mode(const oe_ntt_plan_t *plan);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NTT_INTERNAL_H */
