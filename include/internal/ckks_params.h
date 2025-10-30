#ifndef CKKS_PARAMS_INTERNAL_H
#define CKKS_PARAMS_INTERNAL_H

/* Internal CKKS parameter declarations and lightweight precompute metadata.
 *
 * This header is consumed by the public façade (openfhe_embedded.c) and by
 * internal modules (ntt.c, encode.c, mod_arith.c). It mirrors SEAL-Embedded’s
 * approach: select NTT-friendly primes for a given N, validate constraints,
 * and derive per-prime runtime constants with minimal memory commitment.
 *
 * Key design points inspired by SEAL-Embedded:
 *  - Operate RNS-per-prime to keep live RAM O(N), not O(N * L).
 *  - Choose qi < 2^31 and qi ≡ 1 (mod 2N) for NTT existence on each qi.
 *  - Precompute as little as possible by default (on-the-fly modes), while
 *    exposing size hints if precomputed plans are enabled.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "openfhe_embedded.h" /* for oe_ckks_params_t, enums, status */

/* ============================================================
 * Constraints & compile-time knobs
 * ============================================================ */

/* SEAL-Embedded targets ~30-bit primes on Cortex-M4 for fast 32x32->64 ops. */
#ifndef OE_Q_MAX_BITS
#define OE_Q_MAX_BITS 30
#endif

/* ============================================================
 * Derived runtime constants (per-prime)
 * ============================================================ */

/* Minimal per-prime runtime constants for modular arithmetic and NTT.
 * We keep this generic; exact use is up to mod_arith/ntt implementations.
 */
typedef struct
{
  uint32_t q;           /* modulus */
  uint64_t barrett_mu;  /* floor(2^64 / q), for 64-bit Barrett reduction */
  uint32_t n_inv_mod_q; /* N^{-1} mod q (for inverse NTT scaling) */
  /* Optional fields reserved for future use (e.g., Montgomery R, R^2): */
  uint32_t reserved0;
  uint32_t reserved1;
} oe_prime_runtime_t;

typedef struct
{
  uint32_t ring_dim; /* N */
  uint32_t twoN;     /* 2N */
  uint32_t num_q;    /* number of RNS primes */
  oe_prime_runtime_t primes[OE_MAX_LIMBS];
} oe_derived_consts_t;

/* ============================================================
 * Preset completion & validation
 * ============================================================ */

/* Complete an N=2048 CKKS preset if caller left num_q==0 or qi[] empty.
 * - Fills a small set of NTT-friendly qi (qi ≡ 1 mod 2N), qi < 2^OE_Q_MAX_BITS.
 * - Sets a conservative default scale if not provided.
 * - Leaves mode knobs as-is (caller config), unless unset.
 *
 * Return:
 *  OE_OK on success, or an error if ring_dim unsupported.
 */
oe_status_t ckks_params_get_preset_2048(oe_ckks_params_t *io_params);

/* Validate CKKS params are self-consistent and NTT-friendly:
 * - ring_dim is a power of two
 * - 0 < num_q <= OE_MAX_LIMBS
 * - each qi < 2^31 and qi ≡ 1 (mod 2N)
 * - scale > 0
 */
oe_status_t ckks_params_validate(const oe_ckks_params_t *params);

/* Small helper: check qi ≡ 1 (mod 2N). */
bool ckks_qi_is_ntt_friendly(uint32_t qi, uint32_t ring_dim);

/* ============================================================
 * Derived constants & precompute sizing
 * ============================================================ */

/* Derive per-prime runtime constants (Barrett mu, N^{-1} mod q, etc.). */
oe_status_t ckks_params_derive(const oe_ckks_params_t *params,
                               oe_derived_consts_t *out);

/* Estimate memory needed for NTT plan (twiddle tables, metadata) given mode.
 * When mode == OE_NTT_ON_THE_FLY, this may return 0.
 * When mode == OE_NTT_PRECOMPUTED, returns bytes for per-prime roots/tables.
 */
size_t ckks_params_estimate_ntt_plan_bytes(uint32_t ring_dim,
                                           uint32_t num_q,
                                           const uint32_t *q,
                                           oe_ntt_mode_t mode);

/* Estimate memory needed for IFFT (encoding) twiddles/permutations given mode.
 * When mode == OE_IFFT_ON_THE_FLY, this may return 0.
 * When mode == OE_IFFT_PRECOMPUTED, returns bytes for complex twiddles/index maps.
 */
size_t ckks_params_estimate_ifft_plan_bytes(uint32_t ring_dim,
                                            oe_ifft_mode_t mode);

/* Convenience: do modes imply precomputed tables? */
static inline bool ckks_params_require_precomputed_ntt(oe_ntt_mode_t m)
{
  return (m == OE_NTT_PRECOMPUTED);
}
static inline bool ckks_params_require_precomputed_ifft(oe_ifft_mode_t m)
{
  return (m == OE_IFFT_PRECOMPUTED);
}

#endif /* CKKS_PARAMS_INTERNAL_H */
