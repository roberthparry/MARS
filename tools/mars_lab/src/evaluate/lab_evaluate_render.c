/**
 * @file lab_evaluate_render.c
 * @brief Bounded, shell-free mathematical TeX rendering for native MARS Lab.
 *
 * Only an explicit mathematical command and environment vocabulary is accepted.
 * File input, macro definitions, category-code changes and TeX character escapes
 * are rejected before execution. latex runs without shell escape in a private
 * temporary directory with paranoid kpathsea file access. dvisvgm ignores DVI
 * specials. Native file APIs own document/output access and cleanup; mkdtemp is
 * used solely for atomic creation of the private directory. Unsupported TeX is
 * returned as a rendering diagnostic without altering mathematical results.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <string.h>

#include "file.h"
#include "lab_evaluate_internal.h"
#include "lab_process.h"

/* Sorted mathematical vocabulary. No expansion, I/O or definition primitives. */
static const char *const commands[] = {
    "Alpha",
    "Beta",
    "Big",
    "Bigg",
    "Biggl",
    "Biggr",
    "Bigl",
    "Bigr",
    "Delta",
    "Gamma",
    "Im",
    "Lambda",
    "Leftarrow",
    "Leftrightarrow",
    "Omega",
    "Phi",
    "Pi",
    "Psi",
    "Re",
    "Rightarrow",
    "Sigma",
    "Theta",
    "Upsilon",
    "Xi",
    "aleph",
    "alpha",
    "approx",
    "arccos",
    "arcsin",
    "arctan",
    "arg",
    "ast",
    "asymp",
    "bar",
    "begin",
    "beta",
    "big",
    "bigg",
    "biggl",
    "biggr",
    "bigl",
    "bigr",
    "binom",
    "bmod",
    "boldsymbol",
    "bot",
    "boxed",
    "bullet",
    "cap",
    "cases",
    "cdot",
    "cdots",
    "chi",
    "circ",
    "colon",
    "cos",
    "cosh",
    "cot",
    "coth",
    "csc",
    "cup",
    "ddot",
    "dfrac",
    "diag",
    "displaystyle",
    "div",
    "dot",
    "dots",
    "ell",
    "emptyset",
    "end",
    "epsilon",
    "equiv",
    "eta",
    "exists",
    "exp",
    "frac",
    "gamma",
    "ge",
    "geq",
    "geqslant",
    "hat",
    "hbar",
    "hline",
    "hom",
    "hspace",
    "idotsint",
    "iff",
    "iiiint",
    "iiint",
    "iint",
    "implies",
    "in",
    "inf",
    "infty",
    "int",
    "iota",
    "kappa",
    "ker",
    "lambda",
    "langle",
    "lceil",
    "ldots",
    "le",
    "left",
    "leftarrow",
    "leftrightarrow",
    "leq",
    "leqslant",
    "lfloor",
    "lg",
    "lim",
    "liminf",
    "limsup",
    "ln",
    "log",
    "longleftarrow",
    "longleftrightarrow",
    "longrightarrow",
    "mapsto",
    "mathbb",
    "mathbf",
    "mathcal",
    "mathfrak",
    "mathit",
    "mathnormal",
    "mathop",
    "mathrm",
    "mathsf",
    "mathtt",
    "max",
    "mid",
    "min",
    "mkern",
    "mod",
    "mp",
    "mu",
    "nabla",
    "ne",
    "neg",
    "neq",
    "ni",
    "not",
    "nu",
    "odot",
    "oint",
    "omega",
    "omicron",
    "operatorname",
    "oplus",
    "oslash",
    "otimes",
    "overbrace",
    "overleftarrow",
    "overline",
    "overrightarrow",
    "overset",
    "partial",
    "perp",
    "phantom",
    "phi",
    "pi",
    "pm",
    "pmod",
    "prec",
    "preceq",
    "prime",
    "prod",
    "propto",
    "psi",
    "qquad",
    "quad",
    "rangle",
    "rceil",
    "rfloor",
    "rho",
    "right",
    "rightarrow",
    "sec",
    "setminus",
    "sigma",
    "sim",
    "simeq",
    "sin",
    "sinh",
    "sqrt",
    "square",
    "star",
    "subset",
    "subseteq",
    "substack",
    "succ",
    "succeq",
    "sum",
    "sup",
    "supset",
    "supseteq",
    "tan",
    "tanh",
    "tau",
    "text",
    "textbf",
    "textit",
    "textrm",
    "textsf",
    "textstyle",
    "texttt",
    "tfrac",
    "theta",
    "tilde",
    "times",
    "to",
    "top",
    "underbrace",
    "underline",
    "underset",
    "uparrow",
    "upsilon",
    "varepsilon",
    "varkappa",
    "varphi",
    "varpi",
    "varrho",
    "varsigma",
    "vartheta",
    "vdots",
    "vec",
    "vee",
    "vert",
    "vphantom",
    "wedge",
    "widehat",
    "widetilde",
    "wp",
    "xi",
    "zeta",
};

static int lab_eval_compare_word(const void *key, const void *entry)
{
    return strcmp(key, *(const char *const *)entry);
}

static bool lab_eval_ascii_letter(unsigned char value)
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

static bool lab_eval_permitted_environment(string_cursor_t *cursor)
{
    static const char *const environments[] = {
        "Bmatrix",  "Vmatrix", "aligned", "alignedat",   "array", "bmatrix", "cases",
        "gathered", "matrix",  "pmatrix", "smallmatrix", "split", "vmatrix",
    };
    string_cursor_skip_spaces(cursor);
    if (!string_cursor_consume(cursor, "{"))
        return false;
    string_pos_t start = string_cursor_position(cursor);
    unsigned char byte = 0u;
    while (string_cursor_peek_ascii(cursor, &byte) && lab_eval_ascii_letter(byte))
        string_cursor_next(cursor);
    string_t *environment = string_cursor_extract(start, cursor);
    bool ok = environment && string_cursor_consume(cursor, "}") &&
              bsearch(string_c_str(environment), environments, sizeof(environments) / sizeof(*environments),
                      sizeof(*environments), lab_eval_compare_word);
    string_free(environment);
    return ok;
}

static bool lab_eval_permitted_TeX(const string_t *TeX)
{
    if (!TeX || !string_byte_length(TeX) || string_byte_length(TeX) > 65536u)
        return false;
    string_cursor_t *cursor = string_cursor_new(TeX);
    if (!cursor)
        return false;
    bool ok = true;
    while (ok && !string_cursor_done(cursor)) {
        unsigned char byte = 0u;
        bool ascii = string_cursor_peek_ascii(cursor, &byte);
        if (ascii && ((byte < 32u && byte != '\n' && byte != '\r' && byte != '\t') || byte == 127u)) {
            ok = false;
        } else if (string_cursor_match(cursor, "^^")) {
            ok = false;
        } else if (string_cursor_consume(cursor, "\\")) {
            string_pos_t start = string_cursor_position(cursor);
            if (string_cursor_peek_ascii(cursor, &byte) && lab_eval_ascii_letter(byte)) {
                do {
                    string_cursor_next(cursor);
                } while (string_cursor_peek_ascii(cursor, &byte) && lab_eval_ascii_letter(byte));
                string_t *command = string_cursor_extract(start, cursor);
                ok = command && bsearch(string_c_str(command), commands, sizeof(commands) / sizeof(*commands),
                                        sizeof(*commands), lab_eval_compare_word);
                if (ok && (!strcmp(string_c_str(command), "begin") || !strcmp(string_c_str(command), "end")))
                    ok = lab_eval_permitted_environment(cursor);
                string_free(command);
            } else {
                /* Only fixed mathematical spacing and escaped literal characters. */
                ok = string_cursor_peek_ascii(cursor, &byte) && strchr(" ,;:!{}|_%#$&\\/", byte) != NULL;
                if (ok)
                    string_cursor_next(cursor);
            }
        } else {
            string_cursor_next(cursor);
        }
    }
    string_cursor_free(cursor);
    return ok;
}

static file_t *lab_eval_render_file(const char *directory, const char *name)
{
    string_t *path = string_sprintf("%s/%s", directory, name);
    file_t *file = path ? file_new(path) : NULL;
    string_free(path);
    return file;
}

static void lab_eval_clean_directory(const char *directory)
{
    /* The restricted document can create only these fixed output files. */
    static const char *const names[] = {"expr.tex", "expr.aux", "expr.log", "expr.dvi", "expr.svg"};
    for (size_t i = 0u; i < sizeof(names) / sizeof(*names); ++i) {
        file_t *file = lab_eval_render_file(directory, names[i]);
        if (file) {
            file_delete(file);
            file_free(file);
        }
    }
    file_t *folder = file_new_cstr(directory);
    if (folder) {
        file_remove_directory(folder);
        file_free(folder);
    }
}

static bool lab_eval_namespace_svg(string_t *svg, const char *suffix)
{
    /* dvisvgm uses these attribute forms for glyph IDs and references. */
    static const char *const tokens[] = {"id='", "id=\"", "href='#", "href=\"#", "url(#"};
    for (size_t i = 0u; i < sizeof(tokens) / sizeof(*tokens); ++i) {
        string_t *replacement = string_sprintf("%smars_%s_", tokens[i], suffix);
        bool ok = replacement && string_replace(svg, tokens[i], string_c_str(replacement)) >= 0;
        string_free(replacement);
        if (!ok)
            return false;
    }
    return true;
}

/* Render an allowlisted mathematical document in an isolated working directory. */
string_t *lab_eval_render(const string_t *TeX, string_t **error)
{
    if (error)
        *error = NULL;
    if (!lab_eval_permitted_TeX(TeX)) {
        if (error)
            *error = string_new_with("TeX is empty, too large, or contains an unsupported command or environment");
        return NULL;
    }
    char directory[] = "/tmp/mars-lab-TeX-XXXXXX";
    if (!mkdtemp(directory)) {
        if (error)
            *error = string_new_with("Could not create a private TeX directory");
        return NULL;
    }
    string_t *document = string_sprintf("\\documentclass{article}\n\\pagestyle{empty}\n"
                                        "\\usepackage{amsmath}\n\\usepackage{amssymb}\n"
                                        "\\begin{document}\n\\[\n%s\n\\]\n\\end{document}\n",
                                        string_c_str(TeX));
    file_t *source = lab_eval_render_file(directory, "expr.tex");
    bool ok = source && document && file_write_all_text(source, document);
    file_free(source);
    string_free(document);
    string_t *output = NULL;
    int exit_status = -1;
    const char *latex_argv[] = {"env",
                                "openin_any=p",
                                "openout_any=p",
                                "shell_escape=f",
                                "latex",
                                "-no-shell-escape",
                                "-interaction=nonstopmode",
                                "-halt-on-error",
                                "expr.tex",
                                NULL};
    if (ok)
        ok = lab_proc_run(latex_argv, directory, 10000u, 262144u, &output, &exit_status) && !exit_status;
    if (ok) {
        string_free(output);
        output = NULL;
        const char *svg_argv[] = {"dvisvgm",  "--no-fonts", "--no-specials", "--exact-bbox",
                                  "expr.dvi", "-o",         "expr.svg",      NULL};
        ok = lab_proc_run(svg_argv, directory, 10000u, 262144u, &output, &exit_status) && !exit_status;
    }
    string_t *svg = NULL;
    if (ok) {
        file_t *destination = lab_eval_render_file(directory, "expr.svg");
        file_info_t *info = destination ? file_get_info(destination) : NULL;
        if (info && file_info_size(info) <= 4u * 1024u * 1024u)
            svg = file_read_all_text(destination);
        file_info_free(info);
        file_free(destination);
        if (svg && (!string_byte_length(svg) || !lab_eval_namespace_svg(svg, directory + sizeof(directory) - 7u))) {
            string_free(svg);
            svg = NULL;
        }
    }
    if (!svg && error) {
        *error = output && string_byte_length(output)
                     ? string_clone(output)
                     : string_new_with("TeX rendering failed: check latex/dvisvgm availability and rendering limits");
    }
    string_free(output);
    lab_eval_clean_directory(directory);
    return svg;
}

/* Keep mathematical results usable when optional SVG rendering is unavailable. */
void lab_eval_render_field(json_t *fields, const char *source, const char *destination, const char *error_key)
{
    const string_t *TeX = json_string_value(lab_eval_get(fields, source));
    if (!TeX || !string_byte_length(TeX) || !strcmp(string_c_str(TeX), "(null)"))
        return;
    string_t *error = NULL;
    string_t *svg = lab_eval_render(TeX, &error);
    if (svg)
        lab_eval_set(fields, destination, string_c_str(svg));
    else if (error)
        lab_eval_set(fields, error_key, string_c_str(error));
    string_free(svg);
    string_free(error);
}
