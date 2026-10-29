/**
 * @file TeX_layout.c
 * @brief Layout primitives shared by native mathematical display planners.
 *
 * Owns grouping and aligned-definition syntax while callers retain their native
 * structural ordering and shared-subexpression plans.
 */
#include "TeX-t.h"

#define MARS_SHARED_TeX_INTERNAL_ACCESS
#include "internal/TeX_internal.h"
#define MARS_TeX_INTERNAL_ACCESS
#include "TeX_internal.h"

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

/* Format selected names and already rendered values without inspecting their mathematics. */
string_t *TeX_format_definitions(const string_t *const *names, const TeX_t *const *values, size_t count)
{
    if ((!names || !values) && count)
        return NULL;
    string_t *out = string_new();
    if (!out)
        return NULL;
    for (size_t i = 0u; i < count; ++i) {
        TeX_t *name = names[i] ? TeX_identifier(names[i]) : NULL;
        bool valid = name && values[i] &&
            (!i || !string_append_cstr(out, " \\\\\n")) &&
            string_append_format(out, "%s &= %s", TeX_source(name), TeX_source(values[i])) >= 0;
        TeX_free(name);
        if (!valid) {
            string_free(out);
            return NULL;
        }
    }
    return out;
}
