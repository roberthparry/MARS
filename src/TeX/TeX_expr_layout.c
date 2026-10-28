/**
 * @file TeX_expr_layout.c
 * @brief Layout primitives shared by native mathematical display planners.
 *
 * Owns grouping and aligned-definition syntax while callers retain their native
 * structural ordering and shared-subexpression plans.
 */
#include "TeX-t.h"

#define MARS_SHARED_TeX_INTERNAL_ACCESS
#include "internal/TeX_internal.h"
#define MARS_SHARED_TeX_SEMANTIC_INTERNAL_ACCESS
#include "internal/TeX_semantic_internal.h"
#define MARS_TeX_SEMANTIC_SYMBOLS_INTERNAL_ACCESS
#include "TeX_semantic_symbols.h"
#define MARS_TeX_INTERNAL_ACCESS
#include "TeX_expr_private.h"

/* Own qualification syntax while the caller chooses the associated operand. */
string_t *TeX_qualification(const string_t *operand, const char *caption, TeX_qualification_style_t style)
{
    if (!operand || (style != TeX_QUALIFICATION_FINITE_PART && !caption))
        return NULL;
    if (style == TeX_QUALIFICATION_FINITE_PART)
        return string_sprintf("\\operatorname{Fp}\\!\\left[%S\\right]", operand);
    string_t *plain = string_new_with(caption);
    TeX_t *label = plain ? TeX_text(plain) : NULL;
    string_free(plain);
    if (!label)
        return NULL;
    string_t *result = NULL;
    if (style == TeX_QUALIFICATION_INLINE)
        result = string_sprintf("\\underbrace{%S}_{%s}", operand, TeX_source(label));
    else if (style == TeX_QUALIFICATION_CONDITION)
        result = string_sprintf(";\\;%S:\\;%s", operand, TeX_source(label));
    else if (style == TeX_QUALIFICATION_TRAILING)
        result = string_sprintf("\\quad (%S:\\;%s)", operand, TeX_source(label));
    TeX_free(label);
    return result;
}

/* Supply compact grouping independently of the source mathematics. */
string_t *TeX_parenthesise_compact(const string_t *body)
{
    return body ? string_sprintf("\\left(%S\\right)", body) : NULL;
}

/* Arrange already selected derivation rows without interpreting their mathematics. */
string_t *TeX_align_derivation(const string_t *start, const char *result, const char *alternative,
                               const char *condition, const char *condition_value)
{
    if (!start || (alternative && (!result || !condition || !condition_value)))
        return NULL;
    string_t *out = string_new();
    if (!out)
        return NULL;
    int status;
    if (!result)
        status = string_append_format(out, "\\begin{aligned}[t]\n&%S\n\\end{aligned}", start);
    else if (alternative)
        status = string_append_format(out,
            "\\begin{aligned}[t]\n&%S \\\\\n&= \\begin{cases}\n%s, & %s = %s, \\\\\n%s, & %s \\ne %s.\n"
            "\\end{cases}\n\\end{aligned}", start, alternative, condition, condition_value,
            result, condition, condition_value);
    else
        status = string_append_format(out, "\\begin{aligned}[t]\n&%S \\\\\n&= %s\n\\end{aligned}", start, result);
    if (status >= 0)
        return out;
    string_free(out);
    return NULL;
}

/* Keep alignment syntax in TeX while callers choose and order the mathematical rows. */
string_t *TeX_align_equation_rows(const string_t *definitions, const string_t *left,
                                  const string_t *const *right, size_t count)
{
    if (!definitions || !left || (!right && count))
        return NULL;
    string_t *out = string_new_with("\\begin{aligned} ");
    if (!out || string_append_string(out, definitions) < 0)
        goto fail;
    for (size_t i = 0u; i < count; ++i)
        if (!right[i] || string_append_format(out, " \\\\\n%S &= %S", left, right[i]) < 0)
            goto fail;
    if (string_append_cstr(out, " \\end{aligned}") < 0)
        goto fail;
    return out;
fail:
    string_free(out);
    return NULL;
}

void TeX_emit_definition_heading(sbuf_t *buffer, const char *name, bool separated)
{
    if (separated)
        sbuf_puts(buffer, " \\\\\n");
    TeX_emit_name(buffer, name);
    sbuf_puts(buffer, " &= ");
}

void TeX_emit_parenthesis(sbuf_t *buffer, bool closing)
{
    sbuf_puts(buffer, closing ? "\\right)" : "\\left(");
}

void TeX_emit_product_space(sbuf_t *buffer)
{
    sbuf_puts(buffer, "\\,");
}

int TeX_append_telescoping_product(string_t *out, const char *endpoint)
{
    return string_append_format(out, "\\prod_{k=2}^{%s}\\left(1+\\frac{1}{k}\\right)=\\frac{%s+1}{2}",
                                 endpoint, endpoint);
}

int TeX_append_prime_product(string_t *out, bool positive)
{
    return string_append_cstr(out, positive
        ? "\\prod_{p\\in\\mathbb{P}}\\left(1+\\frac{1}{p}\\right)=+\\infty"
        : "\\prod_{p\\in\\mathbb{P}}\\left(1-\\frac{1}{p}\\right)=0");
}

int TeX_append_trigonometric_sum(string_t *out, const char *endpoint, const char *function,
                                 const char *angle, bool weighted)
{
    string_t *name = function ? string_new_with(function) : NULL;
    const char *spelling = name ? TeX_semantic_function(name) : NULL;
    if (!spelling) {
        string_free(name);
        return -1;
    }
    int result;
    if (weighted)
        result = string_append_format(out, "\\sum_{k=1}^{%s}\\frac{%s(k\\mkern-2mu %s)}{k}",
                                      endpoint, spelling, angle);
    else if (angle)
        result = string_append_format(out, "\\sum_{k=1}^{%s}%s(k\\mkern-2mu %s)", endpoint, spelling, angle);
    else
        result = string_append_format(out, "\\sum_{k=1}^{%s}%s(k)", endpoint, spelling);
    string_free(name);
    return result;
}
