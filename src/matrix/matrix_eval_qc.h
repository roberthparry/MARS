#ifndef MATRIX_EVAL_QC_H
#define MATRIX_EVAL_QC_H

#include "matrix.h"

/*
 * Internal MARS convenience wrappers.
 *
 * These helpers evaluate a matrix to a qcomplex_t snapshot with
 * mat_evaluate_qc(...) and then apply the corresponding numeric matrix
 * function. They are intentionally not part of the public matrix API.
 */

matrix_t *mat_exp_eval_qc(const matrix_t *A);
matrix_t *mat_sin_eval_qc(const matrix_t *A);
matrix_t *mat_cos_eval_qc(const matrix_t *A);
matrix_t *mat_tan_eval_qc(const matrix_t *A);

matrix_t *mat_sinh_eval_qc(const matrix_t *A);
matrix_t *mat_cosh_eval_qc(const matrix_t *A);
matrix_t *mat_tanh_eval_qc(const matrix_t *A);

matrix_t *mat_sqrt_eval_qc(const matrix_t *A);
matrix_t *mat_log_eval_qc(const matrix_t *A);

matrix_t *mat_asin_eval_qc(const matrix_t *A);
matrix_t *mat_acos_eval_qc(const matrix_t *A);
matrix_t *mat_atan_eval_qc(const matrix_t *A);

matrix_t *mat_asinh_eval_qc(const matrix_t *A);
matrix_t *mat_acosh_eval_qc(const matrix_t *A);
matrix_t *mat_atanh_eval_qc(const matrix_t *A);

matrix_t *mat_erf_eval_qc(const matrix_t *A);
matrix_t *mat_erfc_eval_qc(const matrix_t *A);
matrix_t *mat_erfinv_eval_qc(const matrix_t *A);
matrix_t *mat_erfcinv_eval_qc(const matrix_t *A);
matrix_t *mat_gamma_eval_qc(const matrix_t *A);
matrix_t *mat_lgamma_eval_qc(const matrix_t *A);
matrix_t *mat_digamma_eval_qc(const matrix_t *A);
matrix_t *mat_trigamma_eval_qc(const matrix_t *A);
matrix_t *mat_tetragamma_eval_qc(const matrix_t *A);
matrix_t *mat_gammainv_eval_qc(const matrix_t *A);
matrix_t *mat_normal_pdf_eval_qc(const matrix_t *A);
matrix_t *mat_normal_cdf_eval_qc(const matrix_t *A);
matrix_t *mat_normal_logpdf_eval_qc(const matrix_t *A);
matrix_t *mat_lambert_w0_eval_qc(const matrix_t *A);
matrix_t *mat_lambert_wm1_eval_qc(const matrix_t *A);
matrix_t *mat_productlog_eval_qc(const matrix_t *A);
matrix_t *mat_ei_eval_qc(const matrix_t *A);
matrix_t *mat_e1_eval_qc(const matrix_t *A);

#endif
