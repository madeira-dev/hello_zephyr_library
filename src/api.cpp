#include <zephyr/sys/printk.h>
#include "api.h"
#include <exception>

// OpenFHE headers
#include "math/math-hal.h"
#include "lattice/hal/default/dcrtpoly.h"

using namespace lbcrypto;

// --- FIX 1: Restore the DCRTPoly alias ---
// Since we are not including the high-level OpenFHE headers, we must
// manually define what 'DCRTPoly' is.
// In Backend 4, BigVector is defined in math-hal.h, so we just use it.
using DCRTPoly = DCRTPolyImpl<BigVector>;

using usint = uint32_t;

void test_lib()
{
  // Updated print to reflect we are back to Dynamic Backend
  printk("--- OpenFHE Lattice Smoke Test (Dynamic Backend 4) ---\n");

  try
  {
    // Keep N=16 to minimize memory usage, as BE4 is heavy
    usint ringDim = 16;
    printk("1. Generating Parameters (N=%u)...\n", ringDim);

    std::vector<NativeInteger> moduli;
    std::vector<NativeInteger> roots;

    // 29-bit prime
    moduli.push_back(NativeInteger("268435521"));

    for (auto &q : moduli)
    {
      roots.push_back(RootOfUnity(2 * ringDim, q));
    }

    // --- FIX 2: Correct Template Type ---
    // In Backend 4, the main logic uses BigInteger (Dynamic),
    // even though the RNS towers use NativeInteger.
    // So ILDCRTParams<BigInteger> is CORRECT.
    auto params = std::make_shared<ILDCRTParams<BigInteger>>(
        2 * ringDim, moduli, roots);

    printk("   Parameters created.\n");

    printk("2. Creating DCRTPoly...\n");
    DCRTPoly poly1(params, Format::COEFFICIENT);
    poly1 = 1;

    DCRTPoly poly2(params, Format::COEFFICIENT);
    poly2 = 2;

    printk("3. Performing Addition...\n");
    DCRTPoly result = poly1 + poly2;

    // Verify
    if (result.GetElementAtIndex(0).GetValues()[0].ConvertToInt() == 3)
    {
      printk("   [PASS] Polynomial Addition (1+2=3)\n");
    }
    else
    {
      printk("   [FAIL] Polynomial Addition\n");
    }
  }
  catch (const std::exception &e)
  {
    printk("!!! EXCEPTION CAUGHT !!!\n");
    printk("Error message: %s\n", e.what());
  }
  catch (...)
  {
    printk("!!! UNKNOWN EXCEPTION !!!\n");
  }

  printk("--- Lattice Test Complete ---\n");
}
