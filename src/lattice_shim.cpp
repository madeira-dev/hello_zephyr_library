// 1. Critical: Include Math Backend FIRST
#include "math/math-hal.h"

// 2. Critical: Include Parameter Definitions
#include "lattice/hal/default/ilparams.h"

// 3. Include Class Declarations (Headers)
#include "lattice/hal/default/poly.h"
#include "lattice/hal/default/dcrtpoly.h"

// 4. Include Template Implementations (Logic)
// Now that ILNativeParams is defined above, these will compile successfully.
#include "lattice/hal/default/poly-impl.h"
#include "lattice/hal/default/dcrtpoly-impl.h"

// In Backend 2, DCRTPoly relies on NativeVector.
// We only instantiate this one stack.

template class lbcrypto::PolyImpl<NativeVector>;
template class lbcrypto::DCRTPolyImpl<NativeVector>;
