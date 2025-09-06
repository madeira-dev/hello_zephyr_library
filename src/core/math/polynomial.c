#include "string.h"
#include "core/math/polynomial.h"
#include "core/math/math_hal.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(polynomial, LOG_LEVEL_DBG);

// ============================================================================
// Basic Polynomial Operations
// ============================================================================

int poly_init(polynomial_t *poly, uint32_t degree, math_word_t modulus)
{
  if (!poly)
    return -1;

  if (degree > POLY_MAX_DEGREE)
  {
    LOG_ERR("Requested degree %u exceeds maximum %u", degree, POLY_MAX_DEGREE);
    return -1;
  }

  memset(poly->coeffs, 0, sizeof(poly->coeffs));
  poly->degree = degree;
  poly->modulus = modulus;
  poly->is_ntt_form = false;

  return 0;
}

void poly_cleanup(polynomial_t *poly)
{
  if (!poly)
    return;
  // Zero out for security
  memset(poly->coeffs, 0, sizeof(poly->coeffs));
  poly->degree = 0;
  poly->is_ntt_form = false;
}

int poly_copy(polynomial_t *dest, const polynomial_t *src)
{
  if (!dest || !src)
    return -1;

  if (src->degree >= POLY_MAX_DEGREE)
    return -1;

  memcpy(dest->coeffs, src->coeffs, sizeof(src->coeffs));
  dest->degree = src->degree;
  dest->modulus = src->modulus;
  dest->is_ntt_form = src->is_ntt_form;

  return 0;
}

int poly_set_coeff(polynomial_t *poly, uint32_t index, math_word_t value)
{
  if (!poly)
    return -1;

  if (index > poly->degree || index >= POLY_MAX_COEFFS)
  {
    LOG_ERR("Coefficient index %u out of bounds (degree=%u)", index, poly->degree);
    return -1;
  }

  poly->coeffs[index] = value;
  return 0;
}

int poly_get_coeff(const polynomial_t *poly, uint32_t index, math_word_t *result)
{
  if (!poly || !result)
    return -1;

  if (index > poly->degree || index >= POLY_MAX_COEFFS)
  {
    LOG_ERR("Coefficient index %u out of bounds (degree=%u)", index, poly->degree);
    return -1;
  }

  *result = poly->coeffs[index];
  return 0;
}

// ============================================================================
// RNS Polynomial Operations
// ============================================================================

int rns_poly_init(rns_polynomial_t *rpoly, uint32_t degree, uint32_t num_moduli)
{
  if (!rpoly || num_moduli == 0 || num_moduli > RNS_MAX_MODULI)
    return -1;

  rpoly->num_moduli = num_moduli;
  for (uint32_t i = 0; i < num_moduli; i++)
  {
    // Modulus is not known at this point, initialize with 0
    if (poly_init(&rpoly->polys[i], degree, 0) != 0)
      return -1;
  }
  return 0;
}

int rns_poly_copy(rns_polynomial_t *dest, const rns_polynomial_t *src)
{
  if (!dest || !src)
    return -1;

  dest->num_moduli = src->num_moduli;
  for (uint32_t i = 0; i < src->num_moduli; i++)
  {
    if (poly_copy(&dest->polys[i], &src->polys[i]) != 0)
      return -1;
  }
  return 0;
}

int rns_poly_to_ntt(rns_polynomial_t *rpoly, const poly_ring_params_t *params, uint32_t num_moduli)
{
  if (!rpoly || !params)
    return -1;

  for (uint32_t i = 0; i < num_moduli; i++)
  {
    if (poly_to_ntt(&rpoly->polys[i], &params[i].ntt_params) != 0)
      return -1;
  }
  return 0;
}

int rns_poly_from_ntt(rns_polynomial_t *rpoly, const poly_ring_params_t *params, uint32_t num_moduli)
{
  if (!rpoly || !params)
    return -1;

  for (uint32_t i = 0; i < num_moduli; i++)
  {
    if (poly_from_ntt(&rpoly->polys[i], &params[i].ntt_params) != 0)
      return -1;
  }
  return 0;
}

int rns_poly_add(rns_polynomial_t *result, const rns_polynomial_t *a,
                 const rns_polynomial_t *b, const poly_ring_params_t *params, uint32_t num_moduli)
{
  if (!result || !a || !b || !params || num_moduli == 0 || num_moduli > RNS_MAX_MODULI)
    return -1;

  result->num_moduli = num_moduli;
  for (uint32_t i = 0; i < num_moduli; i++)
  {
    if (poly_add(&result->polys[i], &a->polys[i], &b->polys[i], &params[i]) != 0)
      return -1;
  }
  return 0;
}

int rns_poly_sub(rns_polynomial_t *result, const rns_polynomial_t *a,
                 const rns_polynomial_t *b, const poly_ring_params_t *params, uint32_t num_moduli)
{
  if (!result || !a || !b || !params || num_moduli == 0 || num_moduli > RNS_MAX_MODULI)
    return -1;

  result->num_moduli = num_moduli;
  for (uint32_t i = 0; i < num_moduli; i++)
  {
    if (poly_sub(&result->polys[i], &a->polys[i], &b->polys[i], &params[i]) != 0)
      return -1;
  }
  return 0;
}

int rns_poly_mult(rns_polynomial_t *result, const rns_polynomial_t *a,
                  const rns_polynomial_t *b, const poly_ring_params_t *params, uint32_t num_moduli)
{
  if (!result || !a || !b || !params || num_moduli == 0 || num_moduli > RNS_MAX_MODULI)
    return -1;

  result->num_moduli = num_moduli;
  for (uint32_t i = 0; i < num_moduli; i++)
  {
    if (poly_mult(&result->polys[i], &a->polys[i], &b->polys[i], &params[i]) != 0)
      return -1;
  }
  return 0;
}

// ============================================================================
// Arithmetic Operations
// ============================================================================

int poly_add(polynomial_t *result, const polynomial_t *a,
             const polynomial_t *b, const poly_ring_params_t *params)
{
  if (!result || !a || !b || !params)
    return -1;

  uint32_t N = params->ring_dimension;
  math_word_t modulus = params->coefficient_modulus;

  if (poly_init(result, N - 1, modulus) != 0)
    return -1;

  for (uint32_t i = 0; i < N; i++)
  {
    result->coeffs[i] = math_hal_mod_add(a->coeffs[i], b->coeffs[i], modulus);
  }
  result->is_ntt_form = a->is_ntt_form; // Addition preserves NTT form

  return 0;
}

int poly_sub(polynomial_t *result, const polynomial_t *a,
             const polynomial_t *b, const poly_ring_params_t *params)
{
  if (!result || !a || !b || !params)
    return -1;

  uint32_t N = params->ring_dimension;
  math_word_t modulus = params->coefficient_modulus;

  if (poly_init(result, N - 1, modulus) != 0)
    return -1;

  for (uint32_t i = 0; i < N; i++)
  {
    result->coeffs[i] = math_hal_mod_sub(a->coeffs[i], b->coeffs[i], modulus);
  }
  result->is_ntt_form = a->is_ntt_form; // Subtraction preserves NTT form

  return 0;
}

// NTT-based polynomial multiplication
int poly_mult(polynomial_t *result, const polynomial_t *a,
              const polynomial_t *b, const poly_ring_params_t *params)
{
  if (!result || !a || !b || !params)
    return -1;

  uint32_t N = params->ring_dimension;
  math_word_t modulus = params->coefficient_modulus;
  const ntt_params_t *ntt_params = &params->ntt_params;

  // Temporary polynomials for NTT conversion
  // Use static to avoid placing large objects on the stack
  static polynomial_t a_ntt, b_ntt;
  poly_copy(&a_ntt, a);
  poly_copy(&b_ntt, b);

  // 1. Convert operands to NTT form if they are not already
  if (!a_ntt.is_ntt_form)
  {
    if (poly_to_ntt(&a_ntt, ntt_params) != 0)
      return -1;
  }
  if (!b_ntt.is_ntt_form)
  {
    if (poly_to_ntt(&b_ntt, ntt_params) != 0)
      return -1;
  }

  // 2. Perform element-wise multiplication in NTT domain
  if (poly_init(result, N - 1, modulus) != 0)
    return -1;

  if (poly_mult_ntt(result, &a_ntt, &b_ntt, ntt_params) != 0)
    return -1;

  // 3. Convert result back from NTT form to coefficient form
  if (poly_from_ntt(result, ntt_params) != 0)
    return -1;

  return 0;
}

// ============================================================================
// NTT Operations
// ============================================================================

// NTT: Convert polynomial to NTT form
int poly_to_ntt(polynomial_t *poly, const ntt_params_t *ntt_params)
{
  if (!poly || !ntt_params)
    return -1;

  if (math_hal_ntt_forward(poly->coeffs, ntt_params) != 0)
    return -1;

  poly->is_ntt_form = true;
  return 0;
}

// NTT: Convert polynomial from NTT form to coefficient form
int poly_from_ntt(polynomial_t *poly, const ntt_params_t *ntt_params)
{
  if (!poly || !ntt_params)
    return -1;

  if (math_hal_ntt_inverse(poly->coeffs, ntt_params) != 0)
    return -1;

  poly->is_ntt_form = false;
  return 0;
}

// NTT: Multiply two polynomials in NTT form (coefficient-wise)
int poly_mult_ntt(polynomial_t *result, const polynomial_t *a,
                  const polynomial_t *b, const ntt_params_t *ntt_params)
{
  if (!result || !a || !b || !ntt_params)
    return -1;

  if (!a->is_ntt_form || !b->is_ntt_form)
    return -1; // Must be in NTT form

  uint32_t n = ntt_params->n;
  math_word_t modulus = ntt_params->modulus;

  if (math_hal_ntt_mult(result->coeffs, a->coeffs, b->coeffs, ntt_params) != 0)
    return -1;

  result->degree = n - 1;
  result->modulus = modulus;
  result->is_ntt_form = true;
  return 0;
}

// ============================================================================
// Utility Functions
// ============================================================================

bool poly_equals(const polynomial_t *a, const polynomial_t *b)
{
  if (!a || !b)
    return false;

  if (a->degree != b->degree || a->modulus != b->modulus)
    return false;

  for (uint32_t i = 0; i <= a->degree; i++)
  {
    if (a->coeffs[i] != b->coeffs[i])
    {
      return false;
    }
  }

  return true;
}

void poly_zero(polynomial_t *poly)
{
  if (!poly)
    return;

  memset(poly->coeffs, 0, (poly->degree + 1) * sizeof(math_word_t));
}

uint32_t poly_get_degree(const polynomial_t *poly)
{
  if (!poly)
    return 0;

  // Find the highest non-zero coefficient
  for (int i = poly->degree; i >= 0; i--)
  {
    if (poly->coeffs[i] != 0)
    {
      return i;
    }
  }

  return 0; // Polynomial is zero
}