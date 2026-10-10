/**
 * @file lab_evaluate_calculus.c
 * @brief Native derivative and integral result-card projection for MARS Lab.
 *
 * Selects exact worker results, function fallbacks and compact/wrapped TeX for
 * the browser's shared calculus renderer. Uses string_t for line extraction;
 * no expression is evaluated or rewritten here. Missing worker results remain
 * empty so the request controller can distinguish failure from a valid result.
 */
#include "lab_evaluate_internal.h"
#include "lab_presentation.h"
#include "lab_presentation_internal.h"

static const char *lab_pres_calculus_field(const json_t *fields, const char *format, const char *action)
{
    string_t *key = string_sprintf(format, action);
    const char *value = key ? lab_eval_text(fields, string_c_str(key)) : "";
    string_free(key);
    return value;
}

static bool lab_pres_calculus_card(json_t *cards, json_t *lines, const json_t *fields, const char *action,
                                   const char *prefix)
{
    const char *line = lab_eval_text(fields, action);
    if (!*line)
        return true;
    string_t *text = string_new_with(line);
    if (!text)
        return false;
    string_offset_t equals = string_starts_with(text, prefix) ? string_find(text, "=") : -1;
    string_t *body = equals < 0 ? string_new() : lab_pres_slice(text, (size_t)equals + 1u, string_byte_length(text));
    string_t *display = body ? lab_eval_display_bindings(body) : NULL;
    json_t *card = json_new_object();
    bool ok = display && card;
    const char *expression = display ? string_c_str(display) : "";
    const char *function = lab_pres_calculus_field(fields, "%s_function", action);
    const char *short_function = lab_pres_calculus_field(fields, "display_%s_function", action);
    const char *full_function = lab_pres_calculus_field(fields, "full_display_%s_function", action);
    const char *TeX = lab_pres_calculus_field(fields, "%s_TeX", action);
    const char *wrapped = lab_pres_calculus_field(fields, "%s_wrapped_TeX", action);
    const char *value = lab_pres_calculus_field(fields, "%s_value", action);
    ok = ok && lab_eval_set(card, "expression", expression) &&
         lab_eval_set(card, "function", *short_function ? short_function : (*function ? function : expression)) &&
         lab_eval_set(card, "full_function", *full_function ? full_function : (*function ? function : expression)) &&
         lab_eval_set(card, "TeX", TeX) && lab_eval_set(card, "wrapped_TeX", *wrapped ? wrapped : TeX) &&
         lab_eval_set(card, "svg", lab_pres_calculus_field(fields, "%s_svg", action)) &&
         lab_eval_set(card, "wrapped_svg", lab_pres_calculus_field(fields, "%s_wrapped_svg", action)) &&
         lab_eval_set(card, "render_error", lab_pres_calculus_field(fields, "%s_render_error", action)) &&
         lab_eval_set(card, "value", value) &&
         lab_eval_set(card, "value_title", *lab_pres_calculus_field(fields, "%s_values", action) ? "Values" : "Value");
    if (ok && equals >= 0) {
        json_t *entry = json_new_object();
        ok = entry && lab_eval_set(entry, "line", line) && lab_eval_set(entry, "expression", expression);
        if (!lab_pres_append(lines, entry))
            ok = false;
    }
    if (ok)
        ok = lab_eval_put(cards, action, card);
    else
        json_free(card);
    string_free(display);
    string_free(body);
    string_free(text);
    return ok;
}

/* Project both worker result kinds through the same native fallback policy. */
bool lab_pres_calculus_cards(json_t *metadata, const json_t *fields, json_t *lines)
{
    json_t *cards = json_new_object();
    bool ok = cards && lab_pres_calculus_card(cards, lines, fields, "derivative", "d/d") &&
              lab_pres_calculus_card(cards, lines, fields, "integral", "∫d");
    if (ok)
        return lab_eval_put(metadata, "calculus", cards);
    json_free(cards);
    return false;
}
