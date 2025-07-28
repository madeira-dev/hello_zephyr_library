#ifndef OPENFHE_SCHEME_CKKS_ENCRYPTOR_H_
#define OPENFHE_SCHEME_CKKS_ENCRYPTOR_H_

#include "scheme/ckks/ckks.h"
#include "scheme/ckks/ckks_cryptoparams.h"

/**
 * @file ckks_encryptor.h
 * @brief CKKS encryption for OpenFHE-Embedded (encryption-only)
 *
 * Provides encryption of encoded plaintexts using a CKKS public key.
 */

/**
 * @brief Encrypt a CKKS plaintext using the public key
 * @param params CKKS crypto parameters
 * @param pk Public key
 * @param plaintext Encoded plaintext (polynomial)
 * @param ciphertext Output ciphertext (2 polynomials)
 * @return 0 on success, negative on error
 */
int ckks_encrypt(const ckks_cryptoparams_t *params,
                 const ckks_publickey_t *pk,
                 const ckks_plaintext_t *plaintext,
                 ckks_ciphertext_t *ciphertext);

#endif // OPENFHE_SCHEME_CKKS_ENCRYPTOR_H_