#include "openfhe_embedded.h"

#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Minimal skeleton for the public C façade.
 * - Argument validation
 * - Object ownership (alloc/free)
 * - Placeholders mapping to internal modules (mempool, NTT, encode, keygen, encrypt)
 *
 * NOTE: This file intentionally contains no heavy logic. All crypto/NTT/IFFT,
 * memory pool management, and sampling will live in internal modules and be
 * wired in subsequent steps, following SEAL-Embedded’s layering and RNS-per-prime
 * execution strategy.
 * ========================================================================= */

/* ------------------------------
 * Opaque types (PIMPL backing)
 * ------------------------------ */
struct oe_context_s
{
  oe_ckks_params_t params;

  /* Internal state placeholders (to be wired later) */
  void *mempool;       /* memory pool handle */
  size_t mempool_size; /* total bytes reserved */

  /* Flags/state */
  int initialized;
};

struct oe_public_key_s
{
  /* Placeholder for public key material (RNS per-prime storage) */
  void *impl;
};

struct oe_secret_key_s
{
  /* Placeholder for secret key (compressed ternary, 2 bits/coeff) */
  void *impl;
};

struct oe_plaintext_s
{
  /* Placeholder for encoded plaintext polynomial (normal domain) */
  void *impl;
};

struct oe_ciphertext_s
{
  /* Placeholder for ciphertext (two polys, RNS per-prime) */
  void *impl;
};

/* ------------------------------
 * Helpers
 * ------------------------------ */
static int is_power_of_two(uint32_t x)
{
  return x && ((x & (x - 1)) == 0);
}

static int primes_valid(const oe_ckks_params_t *p)
{
  if (!p)
    return 0;
  if (p->num_q > OE_MAX_LIMBS)
    return 0;
  for (uint32_t i = 0; i < p->num_q; ++i)
  {
    if (p->q[i] == 0u)
      return 0;
    if ((p->q[i] >> 31) != 0u)
      return 0; /* must be < 2^31 */
  }
  return 1;
}

/* =========================================================================
 * Parameter preset (PoC-friendly)
 * ========================================================================= */
oe_status_t oe_ckks_default_params_2048(oe_ckks_params_t *out)
{
  if (!out)
    return OE_ERR_INVALID_ARG;
  memset(out, 0, sizeof(*out));

  out->ring_dim = 2048;

  /* For the skeleton, leave primes unspecified (0). In the next steps we will
   * provide a concrete NTT-friendly set (qi ≡ 1 mod 2N) and precompute roots.
   * Keeping num_q=0 allows oe_init() to accept the context while deferring
   * prime selection to a later configuration step if desired.
   */
  out->num_q = 0;

  /* Reasonable default scale for PoC; will be refined during encode wiring. */
  out->scale = 33554432.0 /* 2^25 */;

  /* Start with on-the-fly tables to minimize flash; add precomputed later. */
  out->ntt_mode = OE_NTT_ON_THE_FLY;
  out->ifft_mode = OE_IFFT_ON_THE_FLY;

  out->mempool_hint_bytes = 0; /* let oe_init compute */

  return OE_OK;
}

/* =========================================================================
 * Lifecycle
 * ========================================================================= */
oe_status_t oe_init(const oe_ckks_params_t *params, oe_context_t **out_ctx)
{
  if (!params || !out_ctx)
    return OE_ERR_INVALID_ARG;
  if (!is_power_of_two(params->ring_dim))
    return OE_ERR_INVALID_ARG;
  if (params->scale <= 0.0)
    return OE_ERR_INVALID_ARG;
  if (params->num_q != 0 && !primes_valid(params))
    return OE_ERR_INVALID_ARG;

  oe_context_t *ctx = (oe_context_t *)calloc(1, sizeof(oe_context_t));
  if (!ctx)
    return OE_ERR_OUT_OF_MEMORY;

  ctx->params = *params;

  /* TODO: If num_q == 0, select a preset prime set for the given ring_dim (e.g., N=2048)
   * following SEAL-Embedded guidance (≤30-bit primes, qi ≡ 1 mod 2N) and store in ctx->params.q[].
   */

  /* TODO: Compute/estimate mempool size based on chosen modes and ring_dim,
   * following SEAL-Embedded reuse schedules (encode peak ≥ 16*N bytes, etc.).
   * ctx->mempool = mempool_create(size);
   */
  ctx->mempool = NULL;
  ctx->mempool_size = 0;

  /* TODO: Initialize NTT/IFFT context according to params.ntt_mode/ifft_mode.
   * - If OE_NTT_PRECOMPUTED: precompute/load roots into flash/RAM buffers.
   * - If OE_IFFT_PRECOMPUTED: precompute/load complex roots (double-precision).
   */

  ctx->initialized = 1;
  *out_ctx = ctx;
  return OE_OK;
}

void oe_shutdown(oe_context_t *ctx)
{
  if (!ctx)
    return;

  /* TODO: destroy NTT/IFFT tables if allocated */
  /* TODO: mempool_destroy(ctx->mempool); */

  ctx->mempool = NULL;
  ctx->mempool_size = 0;
  ctx->initialized = 0;

  free(ctx);
}

/* =========================================================================
 * Key management
 * ========================================================================= */
oe_status_t oe_keygen(oe_context_t *ctx,
                      oe_public_key_t **out_pk,
                      oe_secret_key_t **out_sk)
{
  if (!ctx || !out_pk || !out_sk)
    return OE_ERR_INVALID_ARG;
  if (!ctx->initialized)
    return OE_ERR_UNINITIALIZED;

  /* Allocate opaque containers now; fill in later when wiring internals. */
  oe_public_key_t *pk = (oe_public_key_t *)calloc(1, sizeof(oe_public_key_t));
  oe_secret_key_t *sk = (oe_secret_key_t *)calloc(1, sizeof(oe_secret_key_t));
  if (!pk || !sk)
  {
    free(pk);
    free(sk);
    return OE_ERR_OUT_OF_MEMORY;
  }

  /* TODO: keygen flow (SEAL-Embedded style):
   *  - sample ternary secret s (compressed 2 bits/coeff), store in sk->impl
   *  - for each qi: sample uniform a, small error e; compute b = -a*s + e via NTT
   *  - store public key (b,a) per-prime in pk->impl (streaming-friendly layout)
   */

  /* For now, signal not yet implemented so callers don’t proceed unexpectedly. */
  free(pk);
  free(sk);
  return OE_ERR_UNSUPPORTED;
}

void oe_free_public_key(oe_public_key_t *pk)
{
  if (!pk)
    return;
  /* TODO: securely wipe, free per-prime buffers in pk->impl */
  free(pk);
}

void oe_free_secret_key(oe_secret_key_t *sk)
{
  if (!sk)
    return;
  /* TODO: securely wipe compressed secret in sk->impl */
  free(sk);
}

/* =========================================================================
 * Encoding
 * ========================================================================= */
oe_status_t oe_encode_real_f32(oe_context_t *ctx,
                               const float *vec,
                               size_t len,
                               double scale,
                               oe_plaintext_t **out_pt)
{
  if (!ctx || !vec || !out_pt)
    return OE_ERR_INVALID_ARG;
  if (!ctx->initialized)
    return OE_ERR_UNINITIALIZED;
  if (len == 0 || len > (size_t)(ctx->params.ring_dim / 2))
    return OE_ERR_INVALID_ARG;
  if (scale <= 0.0)
    return OE_ERR_INVALID_ARG;

  oe_plaintext_t *pt = (oe_plaintext_t *)calloc(1, sizeof(oe_plaintext_t));
  if (!pt)
    return OE_ERR_OUT_OF_MEMORY;

  /* TODO: encode flow:
   *  - π^{-1} projection indices (on-the-fly or precomputed)
   *  - inverse FFT (double-precision) using ifft_mode
   *  - multiply by scale and round to nearest integer
   *  - store plaintext polynomial (normal domain) in pt->impl (reusing mempool)
   */

  free(pt);
  return OE_ERR_UNSUPPORTED;
}

oe_status_t oe_encode_real_f64(oe_context_t *ctx,
                               const double *vec,
                               size_t len,
                               double scale,
                               oe_plaintext_t **out_pt)
{
  if (!ctx || !vec || !out_pt)
    return OE_ERR_INVALID_ARG;
  if (!ctx->initialized)
    return OE_ERR_UNINITIALIZED;
  if (len == 0 || len > (size_t)(ctx->params.ring_dim / 2))
    return OE_ERR_INVALID_ARG;
  if (scale <= 0.0)
    return OE_ERR_INVALID_ARG;

  oe_plaintext_t *pt = (oe_plaintext_t *)calloc(1, sizeof(oe_plaintext_t));
  if (!pt)
    return OE_ERR_OUT_OF_MEMORY;

  /* TODO: same as f32 path; accept doubles to ease host parity tests. */

  free(pt);
  return OE_ERR_UNSUPPORTED;
}

void oe_free_plaintext(oe_plaintext_t *pt)
{
  if (!pt)
    return;
  /* TODO: wipe polynomial buffer in pt->impl (if any), then free */
  free(pt);
}

/* =========================================================================
 * Encryption (asymmetric)
 * ========================================================================= */
oe_status_t oe_encrypt(oe_context_t *ctx,
                       const oe_public_key_t *pk,
                       const oe_plaintext_t *pt,
                       oe_ciphertext_t **out_ct)
{
  if (!ctx || !pk || !pt || !out_ct)
    return OE_ERR_INVALID_ARG;
  if (!ctx->initialized)
    return OE_ERR_UNINITIALIZED;

  oe_ciphertext_t *ct = (oe_ciphertext_t *)calloc(1, sizeof(oe_ciphertext_t));
  if (!ct)
    return OE_ERR_OUT_OF_MEMORY;

  /* TODO: encrypt flow (RNS-per-prime, memory reuse):
   *  - sample u (ternary), e1, e2 (centered binomial)
   *  - for each qi:
   *      * enter NTT domain as needed (u_i, a_i, b_i, m'_i)
   *      * c0_i = b_i * u_i + e1_i + m'_i
   *      * c1_i = a_i * u_i + e2_i
   *    stream or store per-prime slices in ct->impl
   */

  free(ct);
  return OE_ERR_UNSUPPORTED;
}

void oe_free_ciphertext(oe_ciphertext_t *ct)
{
  if (!ct)
    return;
  /* TODO: wipe and free ct->impl buffers */
  free(ct);
}

/* =========================================================================
 * Export (RAW compact, streaming sink)
 * ========================================================================= */
oe_status_t oe_export_public_key_raw(oe_context_t *ctx,
                                     const oe_public_key_t *pk,
                                     oe_write_fn sink, void *sink_user)
{
  if (!ctx || !pk || !sink)
    return OE_ERR_INVALID_ARG;
  if (!ctx->initialized)
    return OE_ERR_UNINITIALIZED;

  /* TODO: write header {magic,version,scheme=CKKS,ring_dim,num_q,scale}, then q[],
   * then per-prime PK.a and PK.b in normal domain (coefs little-endian).
   * Use sink(...) to stream without large RAM buffers.
   */

  return OE_ERR_UNSUPPORTED;
}

oe_status_t oe_export_secret_key_raw(oe_context_t *ctx,
                                     const oe_secret_key_t *sk,
                                     oe_write_fn sink, void *sink_user)
{
  if (!ctx || !sk || !sink)
    return OE_ERR_INVALID_ARG;
  if (!ctx->initialized)
    return OE_ERR_UNINITIALIZED;

  /* TODO: write header; then 2-bit packed ternary secret (define packing order). */

  return OE_ERR_UNSUPPORTED;
}

oe_status_t oe_export_params_raw(oe_context_t *ctx,
                                 oe_write_fn sink, void *sink_user)
{
  if (!ctx || !sink)
    return OE_ERR_INVALID_ARG;
  if (!ctx->initialized)
    return OE_ERR_UNINITIALIZED;

  /* TODO: write only the parameter header (for host context construction). */

  return OE_ERR_UNSUPPORTED;
}

#ifdef OE_ENABLE_CEREAL
/* =========================================================================
 * Optional on-device cereal (binary)
 * ========================================================================= */
oe_status_t oe_export_public_key_cereal_bin(oe_context_t *ctx,
                                            const oe_public_key_t *pk,
                                            oe_write_fn sink, void *sink_user)
{
  if (!ctx || !pk || !sink)
    return OE_ERR_INVALID_ARG;
  if (!ctx->initialized)
    return OE_ERR_UNINITIALIZED;

  /* TODO: call into C++ bridge to serialize via cereal binary archive and forward
   * bytes to sink(...).
   */

  return OE_ERR_UNSUPPORTED;
}

oe_status_t oe_export_secret_key_cereal_bin(oe_context_t *ctx,
                                            const oe_secret_key_t *sk,
                                            oe_write_fn sink, void *sink_user)
{
  if (!ctx || !sk || !sink)
    return OE_ERR_INVALID_ARG;
  if (!ctx->initialized)
    return OE_ERR_UNINITIALIZED;

  /* TODO: call into C++ bridge (binary archive). */

  return OE_ERR_UNSUPPORTED;
}
#endif /* OE_ENABLE_CEREAL */

/* =========================================================================
 * Introspection
 * ========================================================================= */
uint32_t oe_ring_dim(const oe_context_t *ctx)
{
  return (ctx ? ctx->params.ring_dim : 0u);
}

uint32_t oe_num_primes(const oe_context_t *ctx)
{
  return (ctx ? ctx->params.num_q : 0u);
}

double oe_default_scale(const oe_context_t *ctx)
{
  return (ctx ? ctx->params.scale : 0.0);
}

oe_status_t oe_get_primes(const oe_context_t *ctx, uint32_t *q_out)
{
  if (!ctx || !q_out)
    return OE_ERR_INVALID_ARG;
  for (uint32_t i = 0; i < ctx->params.num_q; ++i)
    q_out[i] = ctx->params.q[i];
  return OE_OK;
}
