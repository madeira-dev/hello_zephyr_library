#ifndef OPENFHE_CORE_UTILS_SERIALIZATION_H_
#define OPENFHE_CORE_UTILS_SERIALIZATION_H_

#include <stdio.h>
#include "scheme/ckks/ckks_cryptoparams.h"

size_t serialization_measure_cryptocontext(const ckks_cryptoparams_t *params);
int serialization_export_cryptocontext(FILE *stream, const ckks_cryptoparams_t *params);
int serialization_export_cryptocontext_path(const char *filepath, const ckks_cryptoparams_t *params);

#endif // OPENFHE_CORE_UTILS_SERIALIZATION_H_