#include "api.h"
#include <exception>
#include <memory>
#include <vector>
#include <zephyr/sys/printk.h>

// OpenFHE headers
#include "lattice/hal/default/dcrtpoly.h"
#include "math/math-hal.h"

using namespace lbcrypto;

// In Backend 2:
// DCRTPoly is strictly DCRTPolyImpl<NativeVector>
using DCRTPoly = DCRTPolyImpl<NativeVector>;
using usint = uint32_t;

void test_lib() {
  printk("--- OpenFHE NATIVE (Backend 2) Test ---\n");

  try {
    usint ringDim = 1024;

    // In Backend 2, NativeInteger is uint64_t.
    // Ensure we are consistent.
    std::vector<NativeInteger> moduli;
    std::vector<NativeInteger> roots;

    NativeInteger q(65537);
    moduli.push_back(q);

    NativeInteger root(6561);
    roots.push_back(root);

    printk("2. Creating Params...\n");
    // FIX: Use DCRTPoly::Params instead of ILNativeParams
    // This uses the typedef inside the DCRTPoly class, which is always correct.
    auto params =
        std::make_shared<DCRTPoly::Params>(2 * ringDim, moduli, roots);

    printk("3. Creating Poly...\n");
    DCRTPoly poly(params, Format::COEFFICIENT);
    poly = 42;

    // Use GetElementAtIndex(0) to get the first RNS tower (NativePoly)
    // Then use [0] to get the first coefficient of that tower
    auto tower = poly.GetElementAtIndex(0);
    NativeInteger val = tower[0];

    printk("   Poly element 0: %u\n", val.ConvertToInt());

    printk("4. Arithmetic...\n");
    DCRTPoly poly2 = poly;
    DCRTPoly sum = poly + poly2;
    auto sumTower = sum.GetElementAtIndex(0);
    NativeInteger sumVal = sumTower[0];

    printk("   Sum element 0: %u (Expected 84)\n", sumVal.ConvertToInt());

    printk("5. NTT Transform...\n");
    sum.SwitchFormat();

    if (sum.GetFormat() == Format::EVALUATION) {
      printk("   [PASS] NTT successful.\n");
    } else {
      printk("   [FAIL] NTT failed.\n");
    }

    printk("--- [SUCCESS] Backend 2 Running! ---\n");
  } catch (const std::exception &e) {
    printk("Error: %s\n", e.what());
  }
}
