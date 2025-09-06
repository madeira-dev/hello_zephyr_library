#ifndef OPENFHE_SCHEME_CKKS_H_
#define OPENFHE_SCHEME_CKKS_H_

#include <stdint.h>
#include <stdbool.h>
#include "core/math/polynomial.h"
#include "core/math/biginteger.h"
#include "scheme/ckks/ckks_cryptoparams.h"

/**
 * @file ckks.h
 * @brief CKKS scheme core types for OpenFHE-Embedded (encryption-only)
 *
 * Defines plaintext, ciphertext, and public key structures for CKKS encryption.
 */

// CKKS Plaintext: just a polynomial (already encoded)
typedef struct
{
    rns_polynomial_t poly;
} ckks_plaintext_t;

// CKKS Ciphertext: array of polynomials (CKKS uses 2 for fresh ciphertexts)
#define CKKS_MAX_CIPHERTEXT_PARTS 2

typedef struct
{
    rns_polynomial_t parts[CKKS_MAX_CIPHERTEXT_PARTS]; // c[0], c[1]
    uint32_t level;                                // Level in modulus chain (0 = highest)
    double scaling_factor;                         // Current scaling factor
    uint32_t depth;                                // Multiplicative depth (for future use)
} ckks_ciphertext_t;

// CKKS Public Key: two polynomials (pk[0], pk[1])
typedef struct
{
    rns_polynomial_t pk[2];
} ckks_publickey_t;

// (Optional) CKKS Secret Key: one polynomial (not needed for encryption-only)
typedef struct
{
    polynomial_t sk;
} ckks_secretkey_t;

#endif // OPENFHE_SCHEME_CKKS_H_