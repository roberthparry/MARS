/**
 * @file test_lab_math_cases.c
 * @brief Sequential registration of native mathematical worker regressions.
 *
 * Keeps all ordinary transform and special-function cases ahead of documented
 * examples. Each owning source registers its cases through the shared harness;
 * the worker fixture arena is reset by that source between cases.
 */
#include "test_lab_support.h"

void test_lab_math_cylindrical_bessel_i_cases(void);
void test_lab_math_cylindrical_bessel_i_readme_cases(void);
void test_lab_math_cylindrical_bessel_y_cases(void);
void test_lab_math_cylindrical_bessel_y_readme_cases(void);
void test_lab_math_cylindrical_struve_h_cases(void);
void test_lab_math_cylindrical_struve_h_readme_cases(void);
void test_lab_math_cylindrical_struve_l_cases(void);
void test_lab_math_cylindrical_struve_l_readme_cases(void);
void test_lab_math_fourier_analytic_cases(void);
void test_lab_math_fourier_analytic_readme_cases(void);
void test_lab_math_fourier_asinh_cases(void);
void test_lab_math_fourier_asinh_readme_cases(void);
void test_lab_math_fourier_atan_cases(void);
void test_lab_math_fourier_atan_readme_cases(void);
void test_lab_math_fourier_bessel_cases(void);
void test_lab_math_fourier_bessel_readme_cases(void);
void test_lab_math_fourier_branch_cases(void);
void test_lab_math_fourier_branch_readme_cases(void);
void test_lab_math_fourier_cases(void);
void test_lab_math_fourier_distribution_cases(void);
void test_lab_math_fourier_distribution_readme_cases(void);
void test_lab_math_fourier_gamma_contour_cases(void);
void test_lab_math_fourier_gamma_contour_readme_cases(void);
void test_lab_math_fourier_hyperbolic_cases(void);
void test_lab_math_fourier_hyperbolic_readme_cases(void);
void test_lab_math_fourier_logarithm_cases(void);
void test_lab_math_fourier_logarithm_readme_cases(void);
void test_lab_math_fourier_periodic_cases(void);
void test_lab_math_fourier_periodic_readme_cases(void);
void test_lab_math_fourier_readme_cases(void);
void test_lab_math_fourier_tanh_cases(void);
void test_lab_math_fourier_tanh_readme_cases(void);
void test_lab_math_inverse_elementary_cases(void);
void test_lab_math_inverse_gaussian_cases(void);
void test_lab_math_inverse_general_cases(void);
void test_lab_math_inverse_general_readme_cases(void);
void test_lab_math_inverse_rational_cases(void);
void test_lab_math_inverse_rational_readme_cases(void);
void test_lab_math_inverse_special_cases(void);
void test_lab_math_laplace_circular_cases(void);
void test_lab_math_laplace_circular_readme_cases(void);
void test_lab_math_laplace_derivatives_cases(void);
void test_lab_math_laplace_derivatives_readme_cases(void);
void test_lab_math_laplace_elementary_cases(void);
void test_lab_math_laplace_gaussian_cases(void);
void test_lab_math_laplace_gaussian_readme_cases(void);
void test_lab_math_laplace_special_cases(void);
void test_lab_math_laplace_time_cases(void);
void test_lab_math_laplace_time_readme_cases(void);
void test_lab_math_polynomial_cases(void);
void test_lab_math_polynomial_readme_cases(void);
void test_lab_math_round_trips_cases(void);
void test_lab_math_sgn_cases(void);
void test_lab_math_sgn_readme_cases(void);
void test_lab_math_style_cases(void);
void test_lab_math_style_readme_cases(void);

/* Register every ordinary mathematical worker case before README examples. */
void test_lab_math_cases(void)
{
    test_lab_math_cylindrical_bessel_i_cases();
    test_lab_math_cylindrical_bessel_y_cases();
    test_lab_math_cylindrical_struve_h_cases();
    test_lab_math_cylindrical_struve_l_cases();
    test_lab_math_fourier_analytic_cases();
    test_lab_math_fourier_asinh_cases();
    test_lab_math_fourier_atan_cases();
    test_lab_math_fourier_bessel_cases();
    test_lab_math_fourier_branch_cases();
    test_lab_math_fourier_cases();
    test_lab_math_fourier_distribution_cases();
    test_lab_math_fourier_gamma_contour_cases();
    test_lab_math_fourier_hyperbolic_cases();
    test_lab_math_fourier_logarithm_cases();
    test_lab_math_fourier_periodic_cases();
    test_lab_math_fourier_tanh_cases();
    test_lab_math_inverse_elementary_cases();
    test_lab_math_inverse_gaussian_cases();
    test_lab_math_inverse_general_cases();
    test_lab_math_inverse_rational_cases();
    test_lab_math_inverse_special_cases();
    test_lab_math_laplace_circular_cases();
    test_lab_math_laplace_derivatives_cases();
    test_lab_math_laplace_elementary_cases();
    test_lab_math_laplace_gaussian_cases();
    test_lab_math_laplace_special_cases();
    test_lab_math_laplace_time_cases();
    test_lab_math_polynomial_cases();
    test_lab_math_round_trips_cases();
    test_lab_math_sgn_cases();
    test_lab_math_style_cases();
}

/* Run all mathematical README examples after the ordinary Lab regressions. */
void test_lab_math_readme_cases(void)
{
    test_lab_math_cylindrical_bessel_i_readme_cases();
    test_lab_math_cylindrical_bessel_y_readme_cases();
    test_lab_math_cylindrical_struve_h_readme_cases();
    test_lab_math_cylindrical_struve_l_readme_cases();
    test_lab_math_fourier_analytic_readme_cases();
    test_lab_math_fourier_asinh_readme_cases();
    test_lab_math_fourier_atan_readme_cases();
    test_lab_math_fourier_bessel_readme_cases();
    test_lab_math_fourier_branch_readme_cases();
    test_lab_math_fourier_distribution_readme_cases();
    test_lab_math_fourier_gamma_contour_readme_cases();
    test_lab_math_fourier_hyperbolic_readme_cases();
    test_lab_math_fourier_logarithm_readme_cases();
    test_lab_math_fourier_periodic_readme_cases();
    test_lab_math_fourier_readme_cases();
    test_lab_math_fourier_tanh_readme_cases();
    test_lab_math_inverse_general_readme_cases();
    test_lab_math_inverse_rational_readme_cases();
    test_lab_math_laplace_circular_readme_cases();
    test_lab_math_laplace_derivatives_readme_cases();
    test_lab_math_laplace_gaussian_readme_cases();
    test_lab_math_laplace_time_readme_cases();
    test_lab_math_polynomial_readme_cases();
    test_lab_math_sgn_readme_cases();
    test_lab_math_style_readme_cases();
}
