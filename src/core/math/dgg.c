#include "core/math/dgg.h"
#include <stdlib.h>

// Pre-computed Cumulative Distribution Table (CDT) for sigma = 3.2
// Generated offline and scaled to fit within a 64-bit integer.
// The table represents the cumulative probability for positive integers.
static const uint64_t dgg_cdt_sigma_3_2[] = {
    4523721125L, 8822335241L, 12681226539L, 15919734241L,
    18418582233L, 20148344369L, 21185038333L, 21687343813L,
    21883199385L, 21960213961L, 21986371029L, 21995951893L,
    21999168929L, 22000214425L, 22000536681L, 22000642825L,
    22000676889L, 22000688161L, 22000691817L, 22000693097L,
    22000693513L, 22000693657L, 22000693705L, 22000693721L};

// Max value in the CDT
#define DGG_MAX_VAL (sizeof(dgg_cdt_sigma_3_2) / sizeof(dgg_cdt_sigma_3_2[0]))

int dgg_init(dgg_sampler_t *sampler, double sigma)
{
  if (!sampler)
    return -1;
  // For now, we only support a fixed sigma of 3.2
  sampler->sigma = 3.2;
  return 0;
}

int32_t dgg_generate_integer(const dgg_sampler_t *sampler)
{
  // Generate a random 64-bit value for table lookup
  uint64_t rand_val;
  math_hal_rng_bytes((uint8_t *)&rand_val, sizeof(rand_val));

  // First bit determines the sign
  int32_t sign = (rand_val & 1) ? -1 : 1;

  // Use the rest of the bits to sample from the positive distribution
  uint64_t sample = rand_val >> 1;

  // Binary search on the pre-computed CDT
  int32_t result = 0;
  int32_t low = 0;
  int32_t high = DGG_MAX_VAL - 1;

  while (low <= high)
  {
    int32_t mid = low + (high - low) / 2;
    if (sample > dgg_cdt_sigma_3_2[mid])
    {
      low = mid + 1;
    }
    else
    {
      result = mid;
      high = mid - 1;
    }
  }

  return (result + 1) * sign;
}