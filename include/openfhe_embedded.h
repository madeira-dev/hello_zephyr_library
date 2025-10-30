#ifndef OPENFHE_EMBEDDED_H
#define OPENFHE_EMBEDDED_H

/* Public C facade for the embedded CKKS library.
 * Design mirrors SEAL-Embedded: tiny stable API surface, RNS-per-prime execution,
 * memory pooling/reuse, and optional off-device adapter serialization.
 *
 * NOTE:
 * - This header is the *only* public include for users.
 * - All other headers remain internal and are not installed.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ==============================
 * Version & feature flags
 * ============================== */
#define OE_VERSION_MAJOR 0
#define OE_VERSION_MINOR 1
#define OE_VERSION_PATCH 0

/* Enable on-device cereal binary serialization (C++ bridge).
 * If undefined, only compact RAW export APIs are exposed.
 */
#ifndef OE_ENABLE_CEREAL
/* leave undefined by default to keep MCU binary small */
#endif

  /* ==============================
   * Status & error reporting
   * ============================== */
  typedef enum
  {
    OE_OK = 0,
    OE_ERR_INVALID_ARG,
    OE_ERR_UNINITIALIZED,
    OE_ERR_OUT_OF_MEMORY,
    OE_ERR_INTERNAL,
    OE_ERR_UNSUPPORTED,
    OE_ERR_BUFFER_TOO_SMALL,
    OE_ERR_SERIALIZATION
  } oe_status_t;

  /* ==============================
   * Opaque handles (PIMPL)
   * ============================== */
  typedef struct oe_context_s oe_context_t;
  typedef struct oe_public_key_s oe_public_key_t;
  typedef struct oe_secret_key_s oe_secret_key_t;
  typedef struct oe_plaintext_s oe_plaintext_t;
  typedef struct oe_ciphertext_s oe_ciphertext_t;

/* ==============================
 * Parameter preset (CKKS-only)
 * ============================== */
/* Minimal CKKS params for PoC. For nRF52840, start with N=2048 to keep RAM low.
   RNS primes kept <= 30 bits (SEAL-Embedded uses up to 30-bit primes on M4). */
#define OE_MAX_LIMBS 8 /* max number of RNS primes supported by the build */

  typedef enum
  {
    OE_NTT_ON_THE_FLY = 0, /* compute roots at runtime: 0 KB tables, slower */
    OE_NTT_PRECOMPUTED = 1 /* store/load roots (flash/RAM): faster, bigger */
  } oe_ntt_mode_t;

  typedef enum
  {
    OE_IFFT_ON_THE_FLY = 0, /* compute twiddles at runtime, minimal flash */
    OE_IFFT_PRECOMPUTED = 1 /* precompute & store (flash), faster */
  } oe_ifft_mode_t;

  typedef struct
  {
    /* Ring dimension (power of two). Start with 2048 for PoC. */
    uint32_t ring_dim;

    /* Number of RNS primes used during encryption (not counting any eval/rescale extras). */
    uint32_t num_q;

    /* RNS primes qi (each < 2^31). Only first num_q entries are used. */
    uint32_t q[OE_MAX_LIMBS];

    /* Scale (Delta) for CKKS encoding (e.g., 2^25 or a prime-sized scale). */
    double scale;

    /* Trade-offs (SEAL-Embedded style knobs): choose memory vs speed. */
    oe_ntt_mode_t ntt_mode;
    oe_ifft_mode_t ifft_mode;

    /* Optional pre-allocated pool size hint (bytes). 0 = auto compute. */
    size_t mempool_hint_bytes;
  } oe_ckks_params_t;

  /* Helper to fill a reasonable preset for nRF52840 PoC (e.g., N=2048, 2–3 primes). */
  oe_status_t oe_ckks_default_params_2048(oe_ckks_params_t *out);

  /* ==============================
   * Library lifecycle
   * ============================== */
  /* One-time initialization with chosen params (allocates pool, precomputes tables as needed). */
  oe_status_t oe_init(const oe_ckks_params_t *params, oe_context_t **out_ctx);

  /* Free all resources associated with ctx (ciphertexts/plaintexts/keys must be destroyed first). */
  void oe_shutdown(oe_context_t *ctx);

  /* ==============================
   * Key management
   * ============================== */
  /* Generate a fresh CKKS keypair on the MCU (ternary secret; public key (b,a)).
     Keys are owned by caller and must be destroyed with oe_free_* when done. */
  oe_status_t oe_keygen(oe_context_t *ctx,
                        oe_public_key_t **out_pk,
                        oe_secret_key_t **out_sk);

  /* Securely wipe and free. */
  void oe_free_public_key(oe_public_key_t *pk);
  void oe_free_secret_key(oe_secret_key_t *sk);

  /* ==============================
   * Encoding (real-valued vectors -> plaintext)
   * ============================== */
  /* Encode len floats into a CKKS plaintext with the given scale.
     Accepts len <= ring_dim/2 (SIMD packing). */
  oe_status_t oe_encode_real_f32(oe_context_t *ctx,
                                 const float *vec,
                                 size_t len,
                                 double scale,
                                 oe_plaintext_t **out_pt);

  /* Optional: double precision input for host parity tests. */
  oe_status_t oe_encode_real_f64(oe_context_t *ctx,
                                 const double *vec,
                                 size_t len,
                                 double scale,
                                 oe_plaintext_t **out_pt);

  /* Destroy plaintext. */
  void oe_free_plaintext(oe_plaintext_t *pt);

  /* ==============================
   * Encryption (asymmetric only)
   * ============================== */
  oe_status_t oe_encrypt(oe_context_t *ctx,
                         const oe_public_key_t *pk,
                         const oe_plaintext_t *pt,
                         oe_ciphertext_t **out_ct);

  /* Destroy ciphertext. */
  void oe_free_ciphertext(oe_ciphertext_t *ct);

  /* ==============================
   * Export interfaces
   * ============================== */
  /* A generic write sink for streaming large outputs without buffering in RAM. */
  typedef size_t (*oe_write_fn)(void *user_ctx, const uint8_t *data, size_t len);

  /* ---- RAW compact export (MCU-friendly) ----
   * A small, endian-tagged binary that packs:
   * [header {magic,version,scheme,ring_dim,num_q,scale}]
   * [q[0..num_q-1]]
   * [PK.a per prime (coeffs in normal domain)]
   * [PK.b per prime (coeffs in normal domain)]
   * Optionally, SK (2-bit packed ternary).
   * A host adapter converts this RAW to OpenFHE objects (mirrors SEAL-Embedded adapter flow).
   */
  oe_status_t oe_export_public_key_raw(oe_context_t *ctx,
                                       const oe_public_key_t *pk,
                                       oe_write_fn sink, void *sink_user);

  oe_status_t oe_export_secret_key_raw(oe_context_t *ctx,
                                       const oe_secret_key_t *sk,
                                       oe_write_fn sink, void *sink_user);

  /* Export the parameter header (for host to build matching OpenFHE context). */
  oe_status_t oe_export_params_raw(oe_context_t *ctx,
                                   oe_write_fn sink, void *sink_user);

#ifdef OE_ENABLE_CEREAL
  /* ---- Optional on-device cereal (binary) ----
   * Requires C++ bridge; may increase flash size. Use only if footprint is acceptable.
   */
  oe_status_t oe_export_public_key_cereal_bin(oe_context_t *ctx,
                                              const oe_public_key_t *pk,
                                              oe_write_fn sink, void *sink_user);

  oe_status_t oe_export_secret_key_cereal_bin(oe_context_t *ctx,
                                              const oe_secret_key_t *sk,
                                              oe_write_fn sink, void *sink_user);
#endif /* OE_ENABLE_CEREAL */

  /* ==============================
   * Introspection (optional, for tests)
   * ============================== */
  uint32_t oe_ring_dim(const oe_context_t *ctx);
  uint32_t oe_num_primes(const oe_context_t *ctx);
  double oe_default_scale(const oe_context_t *ctx);
  oe_status_t oe_get_primes(const oe_context_t *ctx, uint32_t *q_out /* size >= num_q */);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OPENFHE_EMBEDDED_H */
