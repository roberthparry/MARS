#include <stdio.h>

#include "qcomplex.h"
#include "ustring.h"

string_t *qc_to_text(qcomplex_t z) {
    if (qf_cmp(qc_imag(z), qf_from_double(0.0)) == 0) {
        return qf_sprintf_text("%q", qc_real(z));
    } else if (qf_cmp(qc_real(z), qf_from_double(0.0)) == 0) {
        return qf_sprintf_text("%qi", qc_imag(z));
    } else if (qf_cmp(qc_imag(z), qf_from_double(0.0)) < 0) {
        return qf_sprintf_text("%q - %qi", qc_real(z), qf_neg(qc_imag(z)));
    }
    return qf_sprintf_text("%q + %qi", qc_real(z), qc_imag(z));
}

string_t *qc_to_string(qcomplex_t z)
{
    return qc_to_text(z);
}

static qcomplex_t qc_parse_fail_text(const char *msg, const string_t *input)
{
    string_fprintf(stderr, "qc_from_text: %s: \"%S\"\n", msg, input);
    return qc_make(QF_NAN, QF_NAN);
}

static bool qc_has_imag_suffix_text(const string_t *text)
{
    return string_ends_with(text, "i") || string_ends_with(text, "j");
}

static bool qc_text_equals_literal(const string_t *text, const char *literal)
{
    return text && string_view_equals_literal(string_view_all(text), literal);
}

static void qc_normalise_unit_imag_text(string_t *text)
{
    if (!text)
        return;
    if (string_length(text) == 0u || qc_text_equals_literal(text, "+")) {
        string_clear(text);
        string_append_cstr(text, "1");
    } else if (qc_text_equals_literal(text, "-")) {
        string_clear(text);
        string_append_cstr(text, "-1");
    }
}

static int qc_parse_real_imag_parts_text(const string_t *re_text,
                                         const string_t *im_text,
                                         qcomplex_t *out)
{
    qfloat_t re = qf_from_text(re_text);
    qfloat_t im = qf_from_text(im_text);

    if (qf_isnan(re) || qf_isnan(im))
        return -1;

    *out = qc_make(re, im);
    return 0;
}

static string_t *qc_compact_text(const string_t *text)
{
    string_cursor_t *cursor;
    string_t *out;

    if (!text)
        return NULL;

    cursor = string_cursor_new(text);
    out = string_new();
    if (!cursor || !out) {
        string_cursor_free(cursor);
        string_free(out);
        return NULL;
    }

    while (!string_cursor_done(cursor)) {
        string_pos_t before = string_cursor_position(cursor);
        rune_t rune;

        string_cursor_skip_spaces(cursor);
        if (string_cursor_position(cursor) != before)
            continue;

        rune = string_cursor_peek(cursor);
        if (rune_is_none(rune) ||
            string_append_rune(out, rune) != 0 ||
            string_cursor_next(cursor) != 0) {
            string_cursor_free(cursor);
            string_free(out);
            return NULL;
        }
    }

    string_cursor_free(cursor);
    return out;
}

static string_t *qc_slice(const string_t *text, string_pos_t start, string_pos_t end)
{
    if (!text || end < start)
        return NULL;
    return string_substr(text, start, end - start);
}

static string_pos_t qc_text_end(const string_t *text)
{
    return string_view_length(string_view_all(text));
}

static bool qc_find_char(const string_t *text, char ch, string_pos_t *pos_out)
{
    string_cursor_t *cursor = string_cursor_new(text);
    bool found = false;
    unsigned char ascii = 0u;

    if (!cursor)
        return false;

    while (!string_cursor_done(cursor)) {
        if (string_cursor_peek_ascii(cursor, &ascii) &&
            ascii == (unsigned char)ch) {
            if (pos_out)
                *pos_out = string_cursor_position(cursor);
            found = true;
            break;
        }
        if (string_cursor_next(cursor) != 0)
            break;
    }

    string_cursor_free(cursor);
    return found;
}

static bool qc_find_last_char(const string_t *text, char ch, string_pos_t *pos_out)
{
    string_cursor_t *cursor = string_cursor_new(text);
    bool found = false;
    unsigned char ascii = 0u;

    if (!cursor)
        return false;

    while (!string_cursor_done(cursor)) {
        if (string_cursor_peek_ascii(cursor, &ascii) &&
            ascii == (unsigned char)ch) {
            if (pos_out)
                *pos_out = string_cursor_position(cursor);
            found = true;
        }
        if (string_cursor_next(cursor) != 0)
            break;
    }

    string_cursor_free(cursor);
    return found;
}

static bool qc_find_first_signed_split(const string_t *text, string_pos_t *pos_out)
{
    string_cursor_t *cursor = string_cursor_new(text);
    bool found = false;
    unsigned char ascii = 0u;
    bool first = true;

    if (!cursor)
        return false;

    while (!string_cursor_done(cursor)) {
        if (!first &&
            string_cursor_peek_ascii(cursor, &ascii) &&
            (ascii == '+' || ascii == '-')) {
            if (pos_out)
                *pos_out = string_cursor_position(cursor);
            found = true;
            break;
        }
        first = false;
        if (string_cursor_next(cursor) != 0)
            break;
    }

    string_cursor_free(cursor);
    return found;
}

static bool qc_find_last_signed_split(const string_t *text, string_pos_t *pos_out)
{
    string_cursor_t *cursor = string_cursor_new(text);
    bool found = false;
    unsigned char ascii = 0u;
    bool first = true;

    if (!cursor)
        return false;

    while (!string_cursor_done(cursor)) {
        if (!first &&
            string_cursor_peek_ascii(cursor, &ascii) &&
            (ascii == '+' || ascii == '-')) {
            if (pos_out)
                *pos_out = string_cursor_position(cursor);
            found = true;
        }
        first = false;
        if (string_cursor_next(cursor) != 0)
            break;
    }

    string_cursor_free(cursor);
    return found;
}

static string_t *qc_without_imag_suffix(const string_t *text)
{
    string_pos_t end;

    if (!qc_has_imag_suffix_text(text))
        return NULL;

    end = qc_text_end(text);
    return end > 0u ? qc_slice(text, 0u, end - 1u) : string_new();
}

qcomplex_t qc_from_text(const string_t *text)
{
    string_t *compact = qc_compact_text(text);
    string_pos_t end;

    if (!compact)
        return qc_make(QF_NAN, QF_NAN);
    end = qc_text_end(compact);
    if (end == 0u) {
        qcomplex_t fail = qc_parse_fail_text("empty input", text);
        string_free(compact);
        return fail;
    }

    /* ------------------------------------------------------------
       (a,b) tuple
       ------------------------------------------------------------ */
    if (string_starts_with(compact, "(")) {
        string_pos_t comma_pos;
        string_pos_t close_pos;
        string_t *left;
        string_t *right;
        qcomplex_t z;

        if (!qc_find_char(compact, ',', &comma_pos) ||
            !qc_find_last_char(compact, ')', &close_pos) ||
            comma_pos <= 1u ||
            close_pos <= comma_pos) {
            z = qc_parse_fail_text("invalid (a,b) form", text);
            string_free(compact);
            return z;
        }

        left = qc_slice(compact, 1u, comma_pos);
        right = qc_slice(compact, comma_pos + 1u, close_pos);
        if (!left || !right || qc_parse_real_imag_parts_text(left, right, &z) != 0) {
            string_free(left);
            string_free(right);
            z = qc_parse_fail_text("invalid numbers in (a,b)", text);
            string_free(compact);
            return z;
        }

        string_free(left);
        string_free(right);
        string_free(compact);
        return z;
    }

    /* ------------------------------------------------------------
       r*exp(...)
       ------------------------------------------------------------ */
    if (string_find(compact, "exp(") >= 0) {
        string_offset_t exp_pos_offset = string_find(compact, "exp(");
        string_pos_t exp_pos = (string_pos_t)exp_pos_offset;
        string_pos_t star_pos;
        string_pos_t close_pos;
        string_t *radius_text;
        string_t *inside;
        qfloat_t r;

        if (!qc_find_char(compact, '*', &star_pos) ||
            star_pos >= exp_pos ||
            !qc_find_last_char(compact, ')', &close_pos) ||
            close_pos <= exp_pos + 4u) {
            qcomplex_t fail = qc_parse_fail_text("invalid r*exp(...) form", text);
            string_free(compact);
            return fail;
        }

        radius_text = qc_slice(compact, 0u, star_pos);
        inside = qc_slice(compact, exp_pos + 4u, close_pos);
        if (!radius_text || !inside) {
            string_free(radius_text);
            string_free(inside);
            string_free(compact);
            return qc_make(QF_NAN, QF_NAN);
        }

        r = qf_from_text(radius_text);
        string_free(radius_text);
        if (qf_isnan(r)) {
            qcomplex_t fail = qc_parse_fail_text("invalid r in r*exp(...)", text);
            string_free(inside);
            string_free(compact);
            return fail;
        }

        /* --------------------------------------------------------
           FIRST: exp(a+bi) or exp(a-bi)
           -------------------------------------------------------- */
        {
            string_pos_t sign_pos;

            if (qc_find_first_signed_split(inside, &sign_pos)) {
                string_t *left = qc_slice(inside, 0u, sign_pos);
                string_t *right_with_i = qc_slice(inside, sign_pos, qc_text_end(inside));
                string_t *right = qc_without_imag_suffix(right_with_i);
                qcomplex_t e;

                if (!left || !right_with_i || !right) {
                    string_free(left);
                    string_free(right_with_i);
                    string_free(right);
                    string_free(inside);
                    string_free(compact);
                    return qc_parse_fail_text("invalid imaginary part in exp(a+bi)", text);
                }

                if (qc_parse_real_imag_parts_text(left, right, &e) != 0) {
                    string_free(left);
                    string_free(right_with_i);
                    string_free(right);
                    string_free(inside);
                    string_free(compact);
                    return qc_parse_fail_text("invalid numbers in exp(a+bi)", text);
                }

                string_free(left);
                string_free(right_with_i);
                string_free(right);
                string_free(inside);
                string_free(compact);
                return qc_mul(qc_make(r, QF_ZERO), qc_exp(e));
            }
        }

        /* --------------------------------------------------------
           SECOND: exp(theta i)
           -------------------------------------------------------- */
        if (qc_has_imag_suffix_text(inside)) {
            string_t *theta_text = qc_without_imag_suffix(inside);
            qcomplex_t out;
            qfloat_t theta;

            if (!theta_text) {
                string_free(inside);
                string_free(compact);
                return qc_parse_fail_text("invalid angle in exp(theta i)", text);
            }
            if (string_length(theta_text) == 0u ||
                qc_text_equals_literal(theta_text, "+") ||
                qc_text_equals_literal(theta_text, "-")) {
                string_free(theta_text);
                string_free(inside);
                string_free(compact);
                return qc_parse_fail_text("invalid angle in exp(theta i)", text);
            }

            theta = qf_from_text(theta_text);
            string_free(theta_text);
            if (qf_isnan(theta)) {
                string_free(inside);
                string_free(compact);
                return qc_parse_fail_text("invalid angle in exp(theta i)", text);
            }

            qfloat_t re = qf_mul(r, qf_cos(theta));
            qfloat_t im = qf_mul(r, qf_sin(theta));
            out = qc_make(re, im);
            string_free(inside);
            string_free(compact);
            return out;
        }

        string_free(inside);
        {
            qcomplex_t fail = qc_parse_fail_text("invalid exp(...) form", text);
            string_free(compact);
            return fail;
        }
    }

    /* ------------------------------------------------------------
       a ± bi
       ------------------------------------------------------------ */
    {
        string_pos_t split;

        if (qc_find_last_signed_split(compact, &split)) {
            string_t *left = qc_slice(compact, 0u, split);
            string_t *right = qc_slice(compact, split, end);
            string_t *imag = NULL;
            string_t *real = NULL;
            qcomplex_t z;

            if (right && qc_has_imag_suffix_text(right)) {
                imag = qc_without_imag_suffix(right);
                qc_normalise_unit_imag_text(imag);

                if (left && imag &&
                    qc_parse_real_imag_parts_text(left, imag, &z) == 0) {
                    string_free(left);
                    string_free(right);
                    string_free(imag);
                    string_free(compact);
                    return z;
                }
                string_free(imag);
            }

            if (left && qc_has_imag_suffix_text(left)) {
                imag = qc_without_imag_suffix(left);
                real = string_clone(right);
                qc_normalise_unit_imag_text(imag);

                if (real && imag &&
                    qc_parse_real_imag_parts_text(real, imag, &z) == 0) {
                    string_free(left);
                    string_free(right);
                    string_free(real);
                    string_free(imag);
                    string_free(compact);
                    return z;
                }
                string_free(real);
                string_free(imag);
            }

            string_free(left);
            string_free(right);
        }
    }

    /* ------------------------------------------------------------
       Pure imaginary
       ------------------------------------------------------------ */
    if (qc_has_imag_suffix_text(compact)) {
        string_t *zero = string_new_with("0");
        string_t *imag = qc_without_imag_suffix(compact);
        qcomplex_t z;

        qc_normalise_unit_imag_text(imag);
        if (!zero || !imag || qc_parse_real_imag_parts_text(zero, imag, &z) != 0) {
            string_free(zero);
            string_free(imag);
            string_free(compact);
            return qc_parse_fail_text("invalid imaginary number", text);
        }

        string_free(zero);
        string_free(imag);
        string_free(compact);
        return z;
    }

    /* ------------------------------------------------------------
       Pure real
       ------------------------------------------------------------ */
    {
        qfloat_t re = qf_from_text(compact);

        if (qf_isnan(re)) {
            qcomplex_t fail = qc_parse_fail_text("invalid real number", text);
            string_free(compact);
            return fail;
        }

        string_free(compact);
        return qc_make(re, qf_from_double(0.0));
    }
}

qcomplex_t qc_from_string(const char *s)
{
    string_t *text = string_new_with(s ? s : "");
    qcomplex_t result;

    if (!text)
        return qc_make(QF_NAN, QF_NAN);

    result = qc_from_text(text);
    string_free(text);
    return result;
}

/* ------------------------------------------------------------------ */
/*  qc_vsprintf / qc_sprintf / qc_printf                               */
/* ------------------------------------------------------------------ */
