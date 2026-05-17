#include "matrix_internal.h"

matrix_t *mat_charpoly(const matrix_t *A)
{
    if (!A || A->rows != A->cols)
        return NULL;

    if (A->elem == &dval_elem)
        return mat_charpoly_dval(A);

    if (!elem_supports_numeric_algorithms(A->elem))
        return NULL;

    return mat_charpoly_numeric(A);
}

matrix_t *mat_minpoly(const matrix_t *A)
{
    if (!A || A->rows != A->cols)
        return NULL;

    if (A->elem == &dval_elem)
        return mat_minpoly_dval(A);

    return NULL;
}

matrix_t *mat_apply_poly(const matrix_t *A, const matrix_t *coeffs)
{
    matrix_t *R = NULL;

    if (!A || !coeffs || A->rows != A->cols || coeffs->cols != 1 || coeffs->rows == 0)
        return NULL;
    if (A->elem != coeffs->elem)
        return NULL;

    unsigned char c0[MATRIX_SCALAR_STORAGE_BYTES] = {0};
    mat_get(coeffs, 0, 0, c0);
    R = mat_const_identity_with_elem(A->rows, A->elem, c0);
    if (!R)
        return NULL;

    for (size_t k = 1; k < coeffs->rows; ++k) {
        unsigned char ck[MATRIX_SCALAR_STORAGE_BYTES] = {0};
        matrix_t *AR = mat_mul(A, R);
        matrix_t *CI = NULL;
        matrix_t *Next = NULL;

        if (!AR) {
            mat_free(R);
            return NULL;
        }

        mat_get(coeffs, k, 0, ck);
        CI = mat_const_identity_with_elem(A->rows, A->elem, ck);
        if (!CI) {
            mat_free(AR);
            mat_free(R);
            return NULL;
        }

        Next = mat_add(AR, CI);
        mat_free(AR);
        mat_free(CI);
        mat_free(R);
        if (!Next)
            return NULL;
        R = Next;
    }

    return R;
}

matrix_t *mat_schur_complement(const matrix_t *A, size_t split)
{
    matrix_t *A11 = NULL;
    matrix_t *A12 = NULL;
    matrix_t *A21 = NULL;
    matrix_t *A22 = NULL;
    matrix_t *A11_inv = NULL;
    matrix_t *T = NULL;
    matrix_t *P = NULL;
    matrix_t *S = NULL;

    if (!A || A->rows != A->cols || split == 0 || split >= A->rows)
        return NULL;

    A11 = mat_extract_block(A, 0, split, 0, split);
    A12 = mat_extract_block(A, 0, split, split, A->cols - split);
    A21 = mat_extract_block(A, split, A->rows - split, 0, split);
    A22 = mat_extract_block(A, split, A->rows - split, split, A->cols - split);
    if (!A11 || !A12 || !A21 || !A22)
        goto fail;

    A11_inv = mat_inverse(A11);
    if (!A11_inv)
        goto fail;

    T = mat_mul(A21, A11_inv);
    if (!T)
        goto fail;

    P = mat_mul(T, A12);
    if (!P)
        goto fail;

    S = mat_sub(A22, P);

fail:
    mat_free(A11);
    mat_free(A12);
    mat_free(A21);
    mat_free(A22);
    mat_free(A11_inv);
    mat_free(T);
    mat_free(P);
    return S;
}

matrix_t *mat_block_inverse(const matrix_t *A, size_t split)
{
    matrix_t *A11 = NULL, *A12 = NULL, *A21 = NULL, *A22 = NULL;
    matrix_t *A11_inv = NULL, *S = NULL, *S_inv = NULL;
    matrix_t *A11i_A12 = NULL, *A21_A11i = NULL;
    matrix_t *Tmp = NULL, *TL = NULL, *TR = NULL, *BL = NULL, *BR = NULL;
    matrix_t *Out = NULL;

    if (!A || A->rows != A->cols || split == 0 || split >= A->rows)
        return NULL;

    A11 = mat_extract_block(A, 0, split, 0, split);
    A12 = mat_extract_block(A, 0, split, split, A->cols - split);
    A21 = mat_extract_block(A, split, A->rows - split, 0, split);
    A22 = mat_extract_block(A, split, A->rows - split, split, A->cols - split);
    if (!A11 || !A12 || !A21 || !A22)
        goto fail;

    A11_inv = mat_inverse(A11);
    S = mat_schur_complement(A, split);
    if (!A11_inv || !S)
        goto fail;

    S_inv = mat_inverse(S);
    if (!S_inv)
        goto fail;

    A11i_A12 = mat_mul(A11_inv, A12);
    A21_A11i = mat_mul(A21, A11_inv);
    if (!A11i_A12 || !A21_A11i)
        goto fail;

    Tmp = mat_mul(A11i_A12, S_inv);
    if (!Tmp)
        goto fail;
    TR = mat_neg(Tmp);
    mat_free(Tmp);
    Tmp = NULL;
    if (!TR)
        goto fail;

    Tmp = mat_mul(S_inv, A21_A11i);
    if (!Tmp)
        goto fail;
    BL = mat_neg(Tmp);
    mat_free(Tmp);
    Tmp = NULL;
    if (!BL)
        goto fail;

    BR = mat_extract_block(S_inv, 0, S_inv->rows, 0, S_inv->cols);
    if (!BR)
        goto fail;

    Tmp = mat_mul(A11i_A12, BL);
    if (!Tmp)
        goto fail;
    TL = mat_sub(A11_inv, Tmp);
    mat_free(Tmp);
    Tmp = NULL;
    if (!TL)
        goto fail;

    Out = mat_build_block_2x2(TL, TR, BL, BR);

fail:
    mat_free(A11);
    mat_free(A12);
    mat_free(A21);
    mat_free(A22);
    mat_free(A11_inv);
    mat_free(S);
    mat_free(S_inv);
    mat_free(A11i_A12);
    mat_free(A21_A11i);
    mat_free(Tmp);
    mat_free(TL);
    mat_free(TR);
    mat_free(BL);
    mat_free(BR);
    return mat_finalize_symbolic_result(Out);
}

matrix_t *mat_block_solve(const matrix_t *A, const matrix_t *B, size_t split)
{
    matrix_t *A11 = NULL, *A12 = NULL, *A21 = NULL;
    matrix_t *B1 = NULL, *B2 = NULL;
    matrix_t *A11_inv = NULL, *S = NULL, *S_inv = NULL;
    matrix_t *Tmp1 = NULL, *Tmp2 = NULL, *X1 = NULL, *X2 = NULL, *X = NULL;

    if (!A || !B || A->rows != A->cols || A->rows != B->rows)
        return NULL;
    if (split == 0 || split >= A->rows)
        return NULL;

    A11 = mat_extract_block(A, 0, split, 0, split);
    A12 = mat_extract_block(A, 0, split, split, A->cols - split);
    A21 = mat_extract_block(A, split, A->rows - split, 0, split);
    B1 = mat_extract_block(B, 0, split, 0, B->cols);
    B2 = mat_extract_block(B, split, B->rows - split, 0, B->cols);
    if (!A11 || !A12 || !A21 || !B1 || !B2)
        goto fail;

    A11_inv = mat_inverse(A11);
    S = mat_schur_complement(A, split);
    if (!A11_inv || !S)
        goto fail;

    S_inv = mat_inverse(S);
    if (!S_inv)
        goto fail;

    Tmp1 = mat_mul(A11_inv, B1);
    if (!Tmp1)
        goto fail;
    Tmp2 = mat_mul(A21, Tmp1);
    if (!Tmp2)
        goto fail;
    mat_free(Tmp1);
    Tmp1 = mat_sub(B2, Tmp2);
    mat_free(Tmp2);
    Tmp2 = NULL;
    if (!Tmp1)
        goto fail;
    X2 = mat_mul(S_inv, Tmp1);
    mat_free(Tmp1);
    Tmp1 = NULL;
    if (!X2)
        goto fail;

    Tmp1 = mat_mul(A12, X2);
    if (!Tmp1)
        goto fail;
    Tmp2 = mat_sub(B1, Tmp1);
    mat_free(Tmp1);
    Tmp1 = NULL;
    if (!Tmp2)
        goto fail;
    X1 = mat_mul(A11_inv, Tmp2);
    mat_free(Tmp2);
    Tmp2 = NULL;
    if (!X1)
        goto fail;

    X = mat_create_zero_with_elem(A->rows, B->cols, X1->elem);
    if (!X)
        goto fail;
    if (!mat_insert_block(X, 0, 0, X1) || !mat_insert_block(X, split, 0, X2)) {
        mat_free(X);
        X = NULL;
        goto fail;
    }

fail:
    mat_free(A11);
    mat_free(A12);
    mat_free(A21);
    mat_free(B1);
    mat_free(B2);
    mat_free(A11_inv);
    mat_free(S);
    mat_free(S_inv);
    mat_free(Tmp1);
    mat_free(Tmp2);
    mat_free(X1);
    mat_free(X2);
    return mat_finalize_symbolic_result(X);
}
