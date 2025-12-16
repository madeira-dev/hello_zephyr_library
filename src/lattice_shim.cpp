// 1. Critical: Include Math Backend FIRST
#include "math/math-hal.h"

// 2. Critical: Include Parameter Definitions
#include "lattice/hal/default/ilparams.h"

// --- FIX: DEFINE ALIASES HERE (BEFORE IMPLEMENTATIONS) ---
namespace lbcrypto
{
  // This alias is required by poly-impl.h
  using ILNativeParams = ILParamsImpl<NativeInteger>;
}
// ---------------------------------------------------------

// 3. Include Class Declarations (Headers)
#include "lattice/hal/default/poly.h"
#include "lattice/hal/default/dcrtpoly.h"

// 4. Include Template Implementations (Logic)
// Now that ILNativeParams is defined above, these will compile successfully.
#include "lattice/hal/default/poly-impl.h"
#include "lattice/hal/default/dcrtpoly-impl.h"

using namespace lbcrypto;

// 5. Explicit Template Instantiations

// A. Native Poly (Used internally by DCRTPoly)
template class lbcrypto::PolyImpl<NativeVector>;

// B. BigInteger Poly (CRT Composition) - Uses Dynamic Backend
template class lbcrypto::PolyImpl<BigVector>;
template class lbcrypto::DCRTPolyImpl<BigVector>;
