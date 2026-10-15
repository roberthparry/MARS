/**
 * @file lab_evaluate_plot.c
 * @brief Compose native curve SVG with its source formula rendered by the restricted TeX renderer.
 *
 * Small XML trees keep the title and curve in a single scalable, copyable image.
 * Native chart bodies are stream-validated and copied verbatim, avoiding large
 * retained trees and repeated triangle cloning during title composition. No
 * mathematics is interpreted here. The existing bounded TeX service owns its
 * temporary files; composition stays in memory. A renderer failure retains the
 * curve with an escaped plain-text formula and a rendering diagnostic.
 */
#include <math.h>
#include <string.h>

#include "number-t.h"
#include "xml-t.h"
#define MARS_LAB_EVALUATE_INTERNAL_ACCESS
#include "lab_evaluate_internal.h"

static bool attribute(xml_t *node, const char *name, const char *value)
{
    string_t *key = string_new_with(name), *text = string_new_with(value);
    bool ok = key && text && xml_set_attribute(node, key, text);
    string_free(text);
    string_free(key);
    return ok;
}

static bool dimension(xml_t *node, const char *name, unsigned value)
{
    string_t *text = string_sprintf("%u", value);
    bool ok = text && attribute(node, name, string_c_str(text));
    string_free(text);
    return ok;
}

static xml_t *element(const char *name)
{
    string_t *text = string_new_with(name);
    xml_t *node = text ? xml_new_element(text) : NULL;
    string_free(text);
    return node;
}

static xml_t *svg_root(const string_t *markup)
{
    xml_t *document = markup ? xml_from_text(markup) : NULL, *root = NULL;
    /* A bounded document prologue may precede its single SVG element. */
    for (size_t i = 0; i < xml_child_count(document); ++i) {
        const xml_t *child = xml_child_at(document, i);
        if (xml_type(child) == XML_ELEMENT && !strcmp(string_c_str(xml_name(child)), "svg")) {
            root = xml_clone(child);
            break;
        }
    }
    xml_free(document);
    return root;
}

typedef struct {
    const char *text;
    size_t length;
} chart_body_t;

static const char chart_marker[] = "<!--mars-native-chart-body-->";

static bool validate_event(xml_event_t event, const string_t *name, const string_t *text,
                            const dictionary_t *attributes, void *argument)
{
    (void)event;
    (void)name;
    (void)text;
    (void)attributes;
    (void)argument;
    return true;
}

/* Native charts have a bare SVG root. Other documents retain the general tree path. */
static xml_t *chart_root(const string_t *markup, chart_body_t *body)
{
    const char *source = markup ? string_c_str(markup) : NULL;
    size_t length = markup ? string_byte_length(markup) : 0;
    if (length < 12 || strncmp(source, "<svg ", 5) || memcmp(source + length - 6, "</svg>", 6))
        return svg_root(markup);
    const char *end = source + 5;
    char quote = 0;
    /* Only locate the opening tag's end; the XML reader still validates every byte. */
    for (; *end; ++end) {
        if (quote) {
            if (*end == quote)
                quote = 0;
        } else if (*end == '\'' || *end == '"') {
            quote = *end;
        } else if (*end == '>') {
            break;
        }
    }
    if (!*end || end[-1] == '/')
        return svg_root(markup);
    xml_reader_t *reader = xml_reader_new(NULL, validate_event, NULL);
    bool valid = reader && xml_reader_feed(reader, source, length) && xml_reader_finish(reader);
    xml_reader_free(reader);
    if (!valid)
        return NULL;
    string_t *header = string_new();
    bool ok = header && string_append_utf8_exact(header, source, (size_t)(end - source)) == 0 &&
              string_append_cstr(header, "/>") == 0;
    xml_t *chart = ok ? svg_root(header) : NULL;
    string_t *marker = string_new_with("mars-native-chart-body");
    xml_t *placeholder = marker ? xml_new_comment(marker) : NULL;
    ok = chart && placeholder && xml_append_child(chart, placeholder);
    string_free(header);
    string_free(marker);
    xml_free(placeholder);
    if (!ok) {
        xml_free(chart);
        return NULL;
    }
    body->text = end + 1;
    body->length = length - (size_t)(body->text - source) - 6;
    return chart;
}

/* Replace only our generated shell marker; a matching comment inside the body is ordinary chart content. */
static string_t *insert_chart_body(string_t *shell, const chart_body_t *body)
{
    if (!shell || !body->text)
        return shell;
    const char *source = string_c_str(shell), *marker = strstr(source, chart_marker);
    string_t *result = marker ? string_new() : NULL;
    const char *suffix = marker ? marker + sizeof(chart_marker) - 1 : NULL;
    bool ok = result && string_append_utf8_exact(result, source, (size_t)(marker - source)) == 0 &&
              string_append_utf8_exact(result, body->text, body->length) == 0 &&
              string_append_utf8_exact(result, suffix, strlen(suffix)) == 0;
    string_free(shell);
    if (!ok) {
        string_free(result);
        return NULL;
    }
    return result;
}

static bool view_size(const xml_t *node, double *width, double *height)
{
    string_t *key = string_new_with("viewBox");
    const string_t *view = key ? xml_get_attribute(node, key) : NULL;
    size_t count = 0;
    string_t **parts = view ? string_split(view, " ", &count) : NULL;
    bool ok = count == 4;
    if (ok) {
        number_t w = num_create_from_text(parts[2]), h = num_create_from_text(parts[3]);
        *width = num_to_double(w);
        *height = num_to_double(h);
        ok = isfinite(*width) && isfinite(*height) && *width > 0 && *height > 0;
        num_destroy(&h);
        num_destroy(&w);
    }
    string_split_free(parts, count);
    string_free(key);
    return ok;
}

static bool append_text(xml_t *parent, const char *name, const string_t *text, unsigned width)
{
    xml_t *node = element(name), *content = text ? xml_new_text(text) : NULL;
    bool ok = node && content && xml_append_child(node, content);
    if (ok && !strcmp(name, "text"))
        ok = dimension(node, "x", width / 2) && attribute(node, "y", "24") &&
             attribute(node, "text-anchor", "middle") && attribute(node, "font-size", "14") &&
             attribute(node, "fill", "#e6c778");
    ok = ok && xml_append_child(parent, node);
    xml_free(content);
    xml_free(node);
    return ok;
}

/* Use one SVG viewport so fitting, expansion and copying retain the mathematical heading. */
void lab_eval_plot_title(json_t *fields)
{
    const string_t *curve = json_string_value(lab_eval_get(fields, "plot_svg"));
    const string_t *TeX = json_string_value(lab_eval_get(fields, "plot_title_TeX"));
    if (!curve || !TeX)
        return;
    lab_eval_render_field(fields, "plot_title_TeX", "plot_title_svg", "plot_title_render_error");
    chart_body_t body = {0};
    xml_t *chart = chart_root(curve, &body);
    xml_t *title = svg_root(json_string_value(lab_eval_get(fields, "plot_title_svg")));
    xml_t *root = element("svg");
    double cw = 0, ch = 0, tw = 0, th = 0;
    bool ok = chart && root && view_size(chart, &cw, &ch) && cw >= 320 && ch >= 240 && cw <= 8192 && ch <= 8192;
    unsigned width = ok ? (unsigned)cw : 0, height = ok ? (unsigned)ch : 0, title_width = 0, title_height = 0;
    if (title && view_size(title, &tw, &th) && ok) {
        double scale = fmin(1.5, fmin((cw - 32) / tw, 160 / th));
        title_width = (unsigned)ceil(tw * scale);
        title_height = (unsigned)ceil(th * scale);
    } else {
        xml_free(title);
        title = NULL;
    }
    unsigned shift = title_height > 20 ? title_height - 20 : 0;
    string_t *view = string_sprintf("0 0 %u %u", width, height + shift);
    const string_t *plain = json_string_value(lab_eval_get(fields, "plot_title"));
    ok = ok && view && attribute(root, "xmlns", "http://www.w3.org/2000/svg") && attribute(root, "role", "img") &&
         attribute(root, "data-plot-figure", "true") &&
         dimension(root, "width", width) && dimension(root, "height", height + shift) &&
         attribute(root, "viewBox", string_c_str(view)) && append_text(root, "title", plain, width);
    const char *style = "transform:none;margin:0;filter:none;overflow:hidden";
    ok = ok && dimension(chart, "y", shift) && attribute(chart, "style", style) &&
         attribute(chart, "data-plot-chart", "true") && xml_append_child(root, chart);
    if (ok && title)
        ok = dimension(title, "x", (width - title_width) / 2) && dimension(title, "y", 8) &&
             dimension(title, "width", title_width) && dimension(title, "height", title_height) &&
             attribute(title, "fill", "#e6c778") && attribute(title, "color", "#e6c778") &&
             attribute(title, "style", style) && attribute(title, "data-plot-title", "true") &&
             xml_append_child(root, title);
    else if (ok)
        ok = append_text(root, "text", plain, width);
    string_t *combined = ok ? xml_to_string(root) : NULL;
    combined = insert_chart_body(combined, &body);
    if (combined)
        lab_eval_set(fields, "plot_svg", string_c_str(combined));
    string_free(combined);
    string_free(view);
    xml_free(root);
    xml_free(title);
    xml_free(chart);
}
