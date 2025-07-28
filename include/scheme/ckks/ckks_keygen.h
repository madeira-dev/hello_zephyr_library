#ifndef OPENFHE_SCHEME_CKKS_KEYGEN_H_
#define OPENFHE_SCHEME_CKKS_KEYGEN_H_

#include "scheme/ckks/ckks.h"
#include "scheme/ckks/ckks_cryptoparams.h"

/**
 * @file ckks_keygen.h
 * @brief CKKS key generation for OpenFHE-Embedded (encryption-only)
 *
 * Generates public (and optionally secret) keys for CKKS encryption.
 */

/**
 * @brief Generate a CKKS key pair (public and secret key)
 * @param params CKKS crypto parameters
 * @param pk Output public key
 * @param sk Output secret key (can be NULL if not needed)
 * @return 0 on success, negative on error
 */
int ckks_keygen(const ckks_cryptoparams_t *params, ckks_publickey_t *pk, ckks_secretkey_t *sk);

#endif // OPENFHE_SCHEME_CKKS_KEYGEN_H_