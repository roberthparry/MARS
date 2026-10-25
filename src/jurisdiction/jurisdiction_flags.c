/**
 * @file jurisdiction_flags.c
 * @brief Indexed country artwork and independent regional display sequences.
 *
 * Reads optional regional flag metadata without changing jurisdiction membership,
 * calendar rules or time zones. Regional artwork fades to its configured parent
 * and back over fifteen seconds, with 2.5-second fades and five-second holds in a self-contained SVG.
 * No browser timer or network resource is required. Reduced-motion viewers retain the regional frame. Ordinary
 * country and constituent-country flags remain static. Display-only emblems have
 * independent metadata and never create a jurisdiction. Returned strings are owned
 * by the caller under the public jurisdiction API.
 */
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "xml-t.h"
#define MARS_JURISDICTION_INTERNAL_ACCESS
#include "jurisdiction_internal.h"

static string_t *jurisdiction_flag_read(jurisdiction_t *j, const char *code)
{
    sqlite_stmt_t *q = sqlite_stmt_prepare(j->db,
        "select svg from jurisdiction_country_flag where iso_country_code=?1");
    string_t *svg = q && sqlite_stmt_bind_text(q, 1, code) && sqlite_stmt_step(q) == SQLITE_STEP_ROW &&
                    sqlite_stmt_column_bytes(q, 0) <= 1024 * 1024 ?
                    string_new_with(sqlite_stmt_column_text(q, 0)) : NULL;
    sqlite_stmt_finalize(q);
    return svg;
}

static bool flag_attribute(xml_t *element, const char *name, const char *value)
{
    string_t *key = string_new_with(name), *text = string_new_with(value);
    bool ok = key && text && xml_set_attribute(element, key, text);
    string_free(key); string_free(text);
    return ok;
}

static const string_t *flag_attribute_get(const xml_t *element, const char *name)
{
    string_t *key = string_new_with(name);
    const string_t *value = xml_get_attribute(element, key);
    string_free(key);
    return value;
}

static string_t *jurisdiction_flag_frame(const string_t *source, bool regional)
{
    xml_t *document = xml_from_text(source), *root = NULL;
    /* Reviewed offline assets have a single SVG root and disjoint identifiers within each configured pair. */
    for (size_t i = 0; document && i < xml_child_count(document); ++i) {
        const xml_t *node = xml_child_at(document, i);
        if (xml_type(node) == XML_ELEMENT && !strcmp(string_c_str(xml_name(node)), "svg")) root = xml_clone(node);
    }
    bool ok = root != NULL;
    if (ok && !flag_attribute_get(root, "viewBox")) {
        const string_t *width = flag_attribute_get(root, "width"), *height = flag_attribute_get(root, "height");
        char *end_x = NULL, *end_y = NULL;
        double x = width ? strtod(string_c_str(width), &end_x) : 0;
        double y = height ? strtod(string_c_str(height), &end_y) : 0;
        ok = end_x && !*end_x && end_y && !*end_y && isfinite(x) && isfinite(y) && x > 0 && y > 0;
        string_t *box = ok ? string_sprintf("0 0 %.9g %.9g", x, y) : NULL;
        ok = box && flag_attribute(root, "viewBox", string_c_str(box));
        string_free(box);
    }
    ok = ok && flag_attribute(root, "width", "640") && flag_attribute(root, "height", "480") &&
         flag_attribute(root, "preserveAspectRatio", "none") &&
         (!regional || flag_attribute(root, "class", "regional-frame"));
    string_t *result = ok ? xml_to_string(root) : NULL;
    xml_free(root); xml_free(document);
    return result;
}

static string_t *jurisdiction_flag_pair(const string_t *regional, const string_t *parent)
{
    string_t *a = jurisdiction_flag_frame(regional, true), *b = jurisdiction_flag_frame(parent, false);
    string_t *svg = a && b ? string_sprintf(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 640 480\">"
        "<style>@keyframes regionalFlag{0%%,33.333333%%,100%%{opacity:1}50%%,83.333333%%{opacity:0}}"
        ".regional-frame{animation:regionalFlag 15s ease-in-out infinite}"
        "@media(prefers-reduced-motion:reduce){.regional-frame{animation:none}}</style>"
        "%s%s</svg>", string_c_str(b), string_c_str(a)) : NULL;
    string_free(a); string_free(b);
    return svg;
}

/* Resolve display metadata independently of catalogue membership or geographic ownership. */
bool jurisdict_country(jurisdiction_t *j, const char *code, string_t **name, string_t **flag_svg)
{
    if (name) *name = NULL;
    if (flag_svg) *flag_svg = NULL;
    size_t length = code ? strlen(code) : 0;
    if (!j || !name || !flag_svg || name == flag_svg || !code ||
        (length != 2 && (length < 4 || length > 6 || code[2] != '-')) ||
        code[0] < 'A' || code[0] > 'Z' || code[1] < 'A' || code[1] > 'Z') return false;
    for (size_t i = 3; i < length; ++i)
        if ((code[i] < 'A' || code[i] > 'Z') && (code[i] < '0' || code[i] > '9')) return false;
    char parent[7] = {0}, artwork[7] = {0};
    sqlite_stmt_t *q = sqlite_stmt_prepare(j->db,
        "select name,parent_code,artwork_code from jurisdiction_flag_region where code=?1");
    if (q && sqlite_stmt_bind_text(q, 1, code) && sqlite_stmt_step(q) == SQLITE_STEP_ROW) {
        const char *p = sqlite_stmt_column_text(q, 1), *a = sqlite_stmt_column_text(q, 2);
        if (p && a && strlen(p) < sizeof parent && strlen(a) < sizeof artwork) {
            *name = string_new_with(sqlite_stmt_column_text(q, 0));
            memcpy(parent, p, strlen(p) + 1); memcpy(artwork, a, strlen(a) + 1);
        }
    }
    sqlite_stmt_finalize(q);
    if (*name) {
        string_t *regional = jurisdiction_flag_read(j, artwork), *national = jurisdiction_flag_read(j, parent);
        *flag_svg = regional && national ? jurisdiction_flag_pair(regional, national) :
                    string_clone(regional ? regional : national);
        string_free(regional); string_free(national);
        return true;
    }
    q = sqlite_stmt_prepare(j->db, "select name from jurisdiction_flag_emblem where code=?1");
    if (q && sqlite_stmt_bind_text(q, 1, code) && sqlite_stmt_step(q) == SQLITE_STEP_ROW)
        *name = string_new_with(sqlite_stmt_column_text(q, 0));
    sqlite_stmt_finalize(q);
    if (*name) {
        *flag_svg = jurisdiction_flag_read(j, code);
        return true;
    }
    q = sqlite_stmt_prepare(j->db,
        "select name from jurisdiction where jurisdiction_id=?1 and jurisdiction_type in ('country','subdivision')");
    if (q && sqlite_stmt_bind_text(q, 1, code) && sqlite_stmt_step(q) == SQLITE_STEP_ROW)
        *name = string_new_with(sqlite_stmt_column_text(q, 0));
    sqlite_stmt_finalize(q);
    if (!*name) return false;
    *flag_svg = jurisdiction_flag_read(j, code);
    if (length > 2 && !*flag_svg) {
        char country[3] = {code[0], code[1], 0};
        /* Inherit artwork, not the parent's name: subdivisions retain their identity. */
        *flag_svg = jurisdiction_flag_read(j, country);
    }
    return true;
}
