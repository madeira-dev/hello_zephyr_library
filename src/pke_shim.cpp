#include "cryptocontext-impl.h"
#include "lattice/hal/default/dcrtpoly.h"
#include "scheme/ckksrns/ckksrns-cryptocontext.h"

using namespace lbcrypto;

// Force instantiation of the CryptoContext for DCRTPoly
// This triggers the registration of the CKKS scheme.
template class lbcrypto::CryptoContextImpl<DCRTPoly>;