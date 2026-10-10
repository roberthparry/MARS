/**
 * @file lab_worker_ophelia.c
 * @brief Executable frontend for MARS function programs.
 *
 * Linked into the MARS Lab server and dispatched in an isolated calculation process.
 * Parses and executes supported scalar, equation and matrix statements, including function calls, branches and
 * output. Mathematics is delegated to MARS APIs; this is an application interpreter rather than a public library
 * module.
 */

/* Initial Ophelia scalar, equation and matrix frontend. Mathematics stays in the public MARS API. */
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dictionary.h"
#include "equation.h"
#include "expression.h"
#include "matrix.h"
#include "number.h"
#include "ustring.h"
#define MARS_SHARED_EQUATION_INTERNAL_ACCESS
#include "internal/equation_internal.h"
#include "lab_worker_internal.h"

#define SOURCE_LIMIT 65536u
#define SYMBOL_LIMIT 256u
#define PARAMETER_LIMIT 64u
#define DEPTH_LIMIT 32u

typedef struct {
    char *name;
    char *canonical;
    expr_t *expr;
    bool leaf;
    bool constant;
} symbol_t;

typedef struct {
    dictionary_t *index;
    symbol_t entries[SYMBOL_LIMIT];
    size_t count;
} scope_t;

typedef enum { FUNCTION_EXPRESSION, FUNCTION_EQUATION, FUNCTION_MATRIX } function_kind_t;

typedef struct {
    char *name;
    char *parameters[PARAMETER_LIMIT];
    bool constants[PARAMETER_LIMIT];
    size_t count;
    char *body;
    char *end;
    function_kind_t kind;
} function_t;

typedef struct {
    expr_t *expression;
    equation_t *equation;
    equation_solutions_t *solutions;
    matrix_t *matrix;
} value_t;

static void value_free(value_t value)
{
    expr_free(value.expression);
    equ_free(value.equation);
    equ_solutions_free(value.solutions);
    mat_free(value.matrix);
}

static bool value_present(value_t value)
{
    return value.expression || value.equation || value.solutions || value.matrix;
}

typedef struct {
    char *source;
    char *position;
    size_t precision;
    bool failed;
    bool calling;
    size_t statements;
    size_t outputs;
    function_t function;
} runtime_t;

static size_t name_hash(const void *key)
{
    const unsigned char *p = (const unsigned char *)*(const char *const *)key;
    size_t hash = 5381u;
    while (*p)
        hash = hash * 33u + *p++;
    return hash;
}

static int name_compare(const void *left, const void *right)
{
    return strcmp(*(const char *const *)left, *(const char *const *)right);
}

static void fail(runtime_t *runtime, const char *message)
{
    if (!runtime->failed) {
        size_t line = 1u, column = 1u;
        for (const char *p = runtime->source; p < runtime->position && *p; ++p) {
            if (*p == '\n') {
                ++line;
                column = 1u;
            } else {
                ++column;
            }
        }
        fprintf(stderr, "line %zu, column %zu: %s\n", line, column, message);
    }
    runtime->failed = true;
}

static char *copy_range(const char *start, const char *end)
{
    size_t length = (size_t)(end - start);
    char *copy = malloc(length + 1u);
    if (copy) {
        memcpy(copy, start, length);
        copy[length] = '\0';
    }
    return copy;
}

static void space(char **p)
{
    while (isspace((unsigned char)**p))
        ++*p;
}

static bool identifier_start(unsigned char c)
{
    return isalpha(c) || c == '_' || c == '@' || c >= 128u;
}

static bool identifier_part(unsigned char c)
{
    return identifier_start(c) || isdigit(c);
}

/* Bracketed symbolic names are opaque tokens, not arrays or mathematical expressions. */
static char *bracketed_name_end(char *text)
{
    char *end = *text == '[' ? strchr(text + 1, ']') : NULL;
    return end && end != text + 1 ? end + 1 : NULL;
}

static char *identifier(char **p)
{
    space(p);
    if (**p == '[') {
        char *end = bracketed_name_end(*p);
        if (!end)
            return NULL;
        char *name = copy_range(*p, end);
        *p = end;
        return name;
    }
    if (!identifier_start((unsigned char)**p))
        return NULL;
    char *start = *p;
    while (identifier_part((unsigned char)**p))
        ++*p;
    return copy_range(start, *p);
}

static bool take(char **p, char token)
{
    space(p);
    if (**p != token)
        return false;
    ++*p;
    return true;
}

static bool scope_init(scope_t *scope)
{
    memset(scope, 0, sizeof(*scope));
    scope->index = dictionary_create(sizeof(char *), sizeof(size_t), name_hash, name_compare,
                                      NULL, NULL, NULL, NULL, NULL);
    return scope->index != NULL;
}

static void scope_free(scope_t *scope)
{
    dictionary_destroy(scope->index);
    for (size_t i = 0; i < scope->count; ++i) {
        free(scope->entries[i].name);
        free(scope->entries[i].canonical);
        expr_free(scope->entries[i].expr);
    }
}

static symbol_t *lookup(scope_t *scope, const char *name)
{
    size_t index;
    return dictionary_get(scope->index, &name, &index) ? &scope->entries[index] : NULL;
}

static symbol_t *declare(runtime_t *runtime, scope_t *scope, const char *name, bool constant)
{
    symbol_t *symbol = lookup(scope, name);
    if (symbol) {
        if (constant && !symbol->constant)
            fail(runtime, "cannot redeclare an existing variable as const in this prototype");
        return symbol;
    }
    if (scope->count == SYMBOL_LIMIT) {
        fail(runtime, "scalar prototype symbol limit exceeded");
        return NULL;
    }
    expr_bindings_t *bindings = NULL;
    expr_t *parsed = expr_from_function_body(name, &bindings);
    const char *canonical = bindings && expr_bindings_count(bindings) == 1u
                                ? expr_bindings_name_at(bindings, 0u) : NULL;
    if (!parsed || !canonical) {
        fail(runtime, "a declaration requires a variable name, not a built-in constant");
        expr_free(parsed);
        expr_bindings_free(bindings);
        return NULL;
    }
    size_t index = scope->count++;
    symbol = &scope->entries[index];
    symbol->name = strdup(name);
    symbol->canonical = strdup(canonical);
    symbol->constant = constant;
    symbol->leaf = true;
    symbol->expr = constant ? expr_new_named_const(NUM_NAN, canonical) : expr_new_named_var(NUM_NAN, canonical);
    expr_free(parsed);
    expr_bindings_free(bindings);
    if (!symbol->name || !symbol->canonical || !symbol->expr ||
        !dictionary_set(scope->index, &symbol->name, &index)) {
        fail(runtime, "could not allocate a scalar binding");
        return NULL;
    }
    return symbol;
}

/* Register implicit variables, leaving registered calls and named constants to MARS. */
static bool discover(runtime_t *runtime, scope_t *scope, const char *text)
{
    char *p = (char *)text;
    while (*p && !runtime->failed) {
        if (*p == ']' || *p == '{' || *p == '}' || *p == '*' || *p == '?') {
            fail(runtime, "arrays, embedded blocks, convolution and embedded '?' are not supported yet");
            break;
        }
        if (isdigit((unsigned char)*p) || (*p == '.' && isdigit((unsigned char)p[1]))) {
            while (isdigit((unsigned char)*p) || *p == '.')
                ++p;
            if ((*p == 'e' || *p == 'E') &&
                (isdigit((unsigned char)p[1]) || p[1] == '+' || p[1] == '-')) {
                ++p;
                if (*p == '+' || *p == '-')
                    ++p;
                while (isdigit((unsigned char)*p))
                    ++p;
            }
            continue;
        }
        if (*p != '[' && !identifier_start((unsigned char)*p)) {
            ++p;
            continue;
        }
        char *name = identifier(&p);
        if (!name) {
            fail(runtime, "could not read an identifier");
            break;
        }
        char *next = p;
        space(&next);
        if (*next == '(' || strcmp(name, "where") == 0) {
            if (*next == '(' && runtime->function.name && strcmp(name, runtime->function.name) == 0)
                fail(runtime, "nested user-function calls and recursion are not supported yet");
            free(name);
            continue;
        }
        symbol_t *symbol = lookup(scope, name);
        if (!symbol) {
            expr_bindings_t *bindings = NULL;
            expr_t *probe = expr_from_function_body(name, &bindings);
            bool variable = bindings && expr_bindings_count(bindings) != 0u;
            expr_free(probe);
            expr_bindings_free(bindings);
            if (variable)
                symbol = declare(runtime, scope, name, false);
        }
        free(name);
    }
    return !runtime->failed;
}

static void scope_symbols(const scope_t *scope, const char **names, expr_t **symbols)
{
    for (size_t i = 0u; i < scope->count; ++i) {
        names[i] = scope->entries[i].canonical;
        symbols[i] = scope->entries[i].expr;
    }
}

static expr_t *parse_value(runtime_t *runtime, scope_t *scope, const char *text)
{
    if (strcmp(text, "?") == 0 || strcmp(text, "@nan") == 0)
        return expr_new_const(NUM_NAN);
    if (!discover(runtime, scope, text))
        return NULL;
    const char *names[SYMBOL_LIMIT];
    expr_t *symbols[SYMBOL_LIMIT];
    scope_symbols(scope, names, symbols);
    expr_t *value = expr_from_function_body_with_symbols(text, names, symbols, scope->count);
    if (!value)
        fail(runtime, "invalid or unsupported scalar expression");
    return value;
}

static matrix_t *parse_matrix(runtime_t *runtime, scope_t *scope, const char *text)
{
    if (!discover(runtime, scope, text))
        return NULL;
    const char *names[SYMBOL_LIMIT];
    expr_t *symbols[SYMBOL_LIMIT];
    scope_symbols(scope, names, symbols);
    matrix_t *value = mat_from_function_body_with_symbols(text, names, symbols, scope->count);
    if (!value)
        fail(runtime, "invalid or unsupported matrix expression");
    return value;
}

/* Full stops before whitespace terminate statements; internal full stops multiply. */
static char *expression_text(runtime_t *runtime, char **p)
{
    space(p);
    char *start = *p;
    unsigned int depth = 0u;
    while (**p) {
        char c = **p;
        if (c == '[') {
            char *end = bracketed_name_end(*p);
            if (!end) {
                fail(runtime, "expected a complete bracketed variable name");
                return NULL;
            }
            *p = end;
            continue;
        }
        if (c == '(') {
            if (++depth > DEPTH_LIMIT) {
                fail(runtime, "expression nesting limit exceeded");
                return NULL;
            }
        } else if (c == ')') {
            if (!depth)
                break;
            --depth;
        } else if (!depth && (c == ',' || c == '}' ||
                   (c == '.' && (!(*p)[1] || isspace((unsigned char)(*p)[1]) || (*p)[1] == '}')))) {
            break;
        }
        ++*p;
    }
    char *end = *p;
    while (end > start && isspace((unsigned char)end[-1]))
        --end;
    if (depth || end == start) {
        fail(runtime, "expected a complete scalar expression");
        return NULL;
    }
    char *text = copy_range(start, end);
    if (!text)
        fail(runtime, "could not allocate expression text");
    return text;
}

static char *matching_block(runtime_t *runtime, char *p)
{
    unsigned int depth = 1u;
    while (*p) {
        char *name_end = bracketed_name_end(p);
        if (name_end) {
            p = name_end;
            continue;
        }
        if (*p == '{' && ++depth > DEPTH_LIMIT) {
            fail(runtime, "block nesting limit exceeded");
            return NULL;
        }
        if (*p == '}' && --depth == 0u)
            return p;
        ++p;
    }
    fail(runtime, "unclosed function or conditional block");
    return NULL;
}

typedef enum { CONDITION_FALSE, CONDITION_TRUE, CONDITION_UNKNOWN } condition_truth_t;

/* Recognise a complete realpart call, not a prefix of an arithmetic expression. */
static char *realpart_argument(char *text)
{
    space(&text);
    if (strncmp(text, "realpart", 8u) != 0)
        return NULL;
    char *p = text + 8;
    if (!take(&p, '('))
        return NULL;
    char *start = p;
    size_t level = 1u;
    while (*p && level) {
        if (*p == '(')
            ++level;
        if (*p == ')')
            --level;
        if (level)
            ++p;
    }
    if (level)
        return NULL;
    char *end = p++;
    space(&p);
    return *p ? NULL : copy_range(start, end);
}

/* Build native predicates with supplied nodes; no mathematical rewriting belongs in the frontend. */
static expr_t *comparison_guard(runtime_t *runtime, scope_t *scope, char *left_text,
                                 const char *operation, expr_t *left, expr_t *right)
{
    const char *names[] = { "guardleft", "guardright" };
    expr_t *symbols[] = { left, right };
    const char *body = NULL;
    char *argument = realpart_argument(left_text);
    expr_t *inner = argument ? parse_value(runtime, scope, argument) : NULL;
    if (strcmp(operation, "!=") == 0) {
        body = "1 where (guardleft != guardright)";
    } else if (inner && strcmp(operation, ">") == 0) {
        symbols[0] = inner;
        body = "1 where (Re(guardleft) > guardright)";
    } else if (inner && strcmp(operation, "==") == 0) {
        /* Compare native renderings so harmless whitespace does not affect recognition. */
        char *a = expr_to_function_body(inner);
        char *b = expr_to_function_body(right);
        if (a && b && strcmp(a, b) == 0) {
            symbols[0] = inner;
            body = "1 where (real_parameter(guardleft))";
        }
        free(a);
        free(b);
    }
    expr_t *guard = body ? expr_from_function_body_with_symbols(body, names, symbols, 2u) : NULL;
    expr_free(inner);
    free(argument);
    return guard;
}

/* Unknown comparisons retain a native domain predicate where the grammar can express one. */
static condition_truth_t condition_value(runtime_t *runtime, scope_t *scope, char *text, size_t depth,
                                         expr_t **guard)
{
    if (depth > DEPTH_LIMIT) {
        fail(runtime, "condition nesting limit exceeded");
        return CONDITION_FALSE;
    }
    space(&text);
    size_t length = strlen(text);
    while (length && isspace((unsigned char)text[length - 1u]))
        text[--length] = '\0';
    if (*text == '(') {
        int level = 0;
        size_t end = 0u;
        for (; end < length; ++end) {
            char *name_end = bracketed_name_end(text + end);
            if (name_end) {
                end = (size_t)(name_end - text) - 1u;
                continue;
            }
            if (text[end] == '(')
                ++level;
            else if (text[end] == ')' && --level == 0)
                break;
        }
        if (end + 1u == length) {
            text[end] = '\0';
            return condition_value(runtime, scope, text + 1, depth + 1u, guard);
        }
    }
    /* Two passes implement ordinary || / && precedence without inspecting mathematical operands. */
    for (size_t pass = 0u; pass < 2u; ++pass) {
        int level = 0;
        const char *operation = pass ? "&&" : "||";
        for (char *p = text; *p; ++p) {
            char *name_end = bracketed_name_end(p);
            if (name_end) {
                p = name_end - 1;
                continue;
            }
            if (*p == '(')
                ++level;
            else if (*p == ')')
                --level;
            else if (!level && strncmp(p, operation, 2u) == 0) {
                *p = '\0';
                expr_t *a = NULL, *b = NULL;
                condition_truth_t left = condition_value(runtime, scope, text, depth + 1u, &a);
                if (runtime->failed || (pass ? left == CONDITION_FALSE : left == CONDITION_TRUE)) {
                    expr_free(a);
                    return left;
                }
                condition_truth_t right = condition_value(runtime, scope, p + 2, depth + 1u, &b);
                condition_truth_t decisive = pass ? CONDITION_FALSE : CONDITION_TRUE;
                condition_truth_t result = right == decisive ? decisive :
                    left == CONDITION_UNKNOWN || right == CONDITION_UNKNOWN ? CONDITION_UNKNOWN : right;
                if (result == CONDITION_UNKNOWN) {
                    if (left != CONDITION_UNKNOWN) {
                        *guard = b;
                        b = NULL;
                    } else if (right != CONDITION_UNKNOWN) {
                        *guard = a;
                        a = NULL;
                    } else if (pass && a && b) {
                        *guard = expr_with_domain_of(b, a);
                    }
                }
                expr_free(a);
                expr_free(b);
                return result;
            }
        }
    }
    typedef bool (*comparison_t)(number_t, number_t);
    static const struct {
        const char *token;
        comparison_t compare;
        bool negate;
    } comparisons[] = {
        { "==", num_eq, false },
        { "!=", num_eq, true  },
        { ">=", num_ge, false },
        { "<=", num_le, false },
        { ">",  num_gt, false },
        { "<",  num_lt, false }
    };
    int level = 0;
    for (char *p = text; *p; ++p) {
        char *name_end = bracketed_name_end(p);
        if (name_end) {
            p = name_end - 1;
            continue;
        }
        if (*p == '(')
            ++level;
        else if (*p == ')')
            --level;
        else if (!level && strchr("=!<>", *p)) {
            /* The comparison grammar has exactly six operators. */
            for (size_t i = 0; i < sizeof(comparisons) / sizeof(*comparisons); ++i) {
                size_t width = strlen(comparisons[i].token);
                if (strncmp(p, comparisons[i].token, width) != 0)
                    continue;
                *p = '\0';
                expr_t *left = parse_value(runtime, scope, text);
                expr_t *right = parse_value(runtime, scope, p + width);
                condition_truth_t result = CONDITION_FALSE;
                if (left && right) {
                    number_t a = expr_eval(left), b = expr_eval(right);
                    if (num_is_nan(a) || num_is_nan(b)) {
                        result = CONDITION_UNKNOWN;
                        *guard = comparison_guard(runtime, scope, text, comparisons[i].token, left, right);
                    } else {
                        result = comparisons[i].compare(a, b) != comparisons[i].negate
                                     ? CONDITION_TRUE : CONDITION_FALSE;
                    }
                    num_destroy(&a);
                    num_destroy(&b);
                }
                expr_free(left);
                expr_free(right);
                return result;
            }
            fail(runtime, "unsupported comparison operator");
            return CONDITION_FALSE;
        }
    }
    fail(runtime, "expected a scalar comparison in the condition");
    return CONDITION_FALSE;
}

static value_t execute(runtime_t *runtime, scope_t *scope, char *start, char *end, bool function, size_t depth);

static value_t call(runtime_t *runtime, scope_t *scope, char *text, size_t depth)
{
    if (depth > DEPTH_LIMIT) {
        fail(runtime, "value nesting limit exceeded");
        return (value_t){0};
    }
    char *p = text;
    char *name = identifier(&p);
    bool parenthesis = name && take(&p, '(');
    bool constructor = parenthesis && strcmp(name, "equation") == 0;
    bool solve = parenthesis && strcmp(name, "solve") == 0;
    bool user_call = parenthesis && runtime->function.name && strcmp(name, runtime->function.name) == 0;
    free(name);
    if (constructor || solve) {
        char *argument = expression_text(runtime, &p);
        bool closed = take(&p, ')');
        space(&p);
        value_t value = {0};
        if (!argument || !closed || *p) {
            fail(runtime, "expected a complete equation(...) or solve(...) call");
        } else if (solve) {
            value_t input = call(runtime, scope, argument, depth + 1u);
            if (!runtime->failed && !input.equation)
                fail(runtime, "solve requires an equation value");
            if (!runtime->failed && !expr_bindings_count(equ_bindings(input.equation)))
                fail(runtime, "solve requires an equation with a named unknown");
            if (!runtime->failed) {
                value.solutions = equ_derive_solutions(input.equation);
                if (!value.solutions)
                    fail(runtime, "native equation solver failed");
            }
            value_free(input);
        } else {
            char *equals = NULL;
            int level = 0;
            for (char *q = argument; *q; ++q) {
                if (*q == '(') ++level;
                else if (*q == ')') --level;
                else if (!level && *q == '=') {
                    if (equals) {
                        fail(runtime, "equation requires exactly one top-level '='");
                        break;
                    }
                    equals = q;
                }
            }
            if (!equals)
                fail(runtime, "equation requires lhs = rhs");
            if (!runtime->failed) {
                *equals = '\0';
                expr_t *lhs = parse_value(runtime, scope, argument);
                expr_t *rhs = parse_value(runtime, scope, equals + 1);
                if (lhs && rhs) {
                    value.equation = equ_new_with_inferred_bindings(lhs, rhs);
                    if (!value.equation)
                        fail(runtime, "could not construct equation");
                }
                expr_free(lhs);
                expr_free(rhs);
            }
        }
        free(argument);
        return value;
    }
    if (!user_call)
        return (value_t){.expression = parse_value(runtime, scope, text)};
    if (runtime->calling) {
        fail(runtime, "recursion is not supported yet");
        return (value_t){0};
    }
    scope_t local;
    if (!scope_init(&local)) {
        fail(runtime, "could not allocate function scope");
        return (value_t){0};
    }
    function_t *fn = &runtime->function;
    for (size_t i = 0; i < fn->count && !runtime->failed; ++i) {
        char *argument = expression_text(runtime, &p);
        expr_t *value = argument ? parse_value(runtime, scope, argument) : NULL;
        symbol_t *parameter = value ? declare(runtime, &local, fn->parameters[i], fn->constants[i]) : NULL;
        if (parameter) {
            if (fn->constants[i]) {
                number_t number = expr_eval(value);
                expr_set_val(parameter->expr, number);
                num_destroy(&number);
            } else {
                expr_free(parameter->expr);
                parameter->expr = value;
                parameter->leaf = false;
                value = NULL;
            }
        }
        expr_free(value);
        free(argument);
        if (i + 1u < fn->count && !take(&p, ','))
            fail(runtime, "function argument count does not match its declaration");
    }
    if (!take(&p, ')'))
        fail(runtime, "function argument count does not match its declaration");
    space(&p);
    if (*p)
        fail(runtime, "only a complete user-function call is supported here");
    value_t result = {0};
    if (!runtime->failed) {
        runtime->calling = true;
        result = execute(runtime, &local, fn->body, fn->end, true, 0u);
        runtime->calling = false;
        if (!runtime->failed && !value_present(result))
            fail(runtime, "function ended without returning a value");
        bool matching_type = fn->kind == FUNCTION_MATRIX ? result.matrix != NULL :
                             fn->kind == FUNCTION_EQUATION ? result.equation != NULL : result.expression != NULL;
        if (!runtime->failed && !matching_type)
            fail(runtime, "return value does not match the declared function type");
    }
    scope_free(&local);
    return result;
}

static void define_function(runtime_t *runtime, char **p)
{
    function_t *fn = &runtime->function;
    if (fn->name) {
        fail(runtime, "multiple function declarations are not supported yet");
        return;
    }
    fn->name = identifier(p);
    if (!fn->name || !take(p, '(')) {
        fail(runtime, "expected a function name(parameters) { ... }");
        return;
    }
    space(p);
    while (**p && **p != ')' && !runtime->failed) {
        if (fn->count == PARAMETER_LIMIT) {
            fail(runtime, "function parameter limit exceeded");
            return;
        }
        bool constant = false;
        char *name = identifier(p);
        if (name && strcmp(name, "const") == 0) {
            constant = true;
            free(name);
            name = identifier(p);
        }
        if (!name || strcmp(name, "array") == 0) {
            free(name);
            fail(runtime, "only scalar parameters are supported yet");
            return;
        }
        /* At most 64 parameters: this bounded scan diagnoses duplicate declarations only. */
        for (size_t i = 0; i < fn->count; ++i)
            if (strcmp(fn->parameters[i], name) == 0)
                fail(runtime, "duplicate function parameter");
        fn->parameters[fn->count] = name;
        fn->constants[fn->count++] = constant;
        if (!take(p, ','))
            break;
    }
    if (!take(p, ')') || !take(p, '{')) {
        fail(runtime, "expected a function body");
        return;
    }
    fn->body = *p;
    fn->end = matching_block(runtime, *p);
    if (fn->end)
        *p = fn->end + 1;
}

static void output_matrix(runtime_t *runtime, const matrix_t *matrix, bool algebraic)
{
    matrix_t *numeric = algebraic ? NULL : mat_evaluate(matrix);
    bool complete = numeric != NULL;
    for (size_t row = 0u; complete && row < mat_get_row_count(numeric); ++row) {
        for (size_t col = 0u; complete && col < mat_get_col_count(numeric); ++col) {
            number_t entry = mat_get_num(numeric, row, col);
            complete = !num_is_nan(entry);
            num_destroy(&entry);
        }
    }
    if (complete) {
        mat_printf("%.*m\n", (int)runtime->precision, numeric);
    } else {
        char *text = mat_to_string(matrix, MAT_STRING_EXPRESSION);
        if (text && strlen(text) > 120u) {
            char *layout = mat_to_string(matrix, MAT_STRING_EXPRESSION_LAYOUT);

            if (layout) {
                free(text);
                text = layout;
            }
        }
        if (text) {
            puts(text);
            free(text);
        } else {
            fail(runtime, "could not render matrix output");
        }
    }
    mat_free(numeric);
}

static void output_equation(runtime_t *runtime, const equation_t *equation, bool algebraic, bool solution)
{
    expr_t *lhs = expr_beautify(equ_lhs(equation));
    expr_t *rhs = algebraic && solution ? expr_clone(equ_rhs(equation)) : expr_beautify(equ_rhs(equation));
    if (rhs && !algebraic) {
        number_t number = expr_eval(rhs);
        if (!num_is_nan(number)) {
            expr_free(rhs);
            rhs = expr_new_const(number);
        }
        num_destroy(&number);
    }
    equation_t *display = lhs && rhs ? equ_new(lhs, rhs) : NULL;
    if (display)
        equ_printf("%.*nu\n", (int)runtime->precision, display);
    else
        fail(runtime, "could not render equation output");
    equ_free(display);
    expr_free(rhs);
    expr_free(lhs);
}

/* Only the generated undefined fallback permits carrying a domain guard instead of choosing a branch. */
static bool undefined_fallback(char *start, char *end)
{
    if (!start || !end)
        return false;
    char *p = start;
    char *name = identifier(&p);
    bool valid = name && strcmp(name, "return") == 0;
    free(name);
    space(&p);
    if (!valid || strncmp(p, "@nan", 4u) != 0)
        return false;
    p += 4;
    if (!take(&p, '.'))
        return false;
    space(&p);
    return p == end;
}

/* A symbolic branch may replace local bindings, but must not mutate the caller's shared leaf nodes. */
static bool branch_scope(runtime_t *runtime, scope_t *target, const scope_t *source)
{
    if (!scope_init(target)) {
        fail(runtime, "could not allocate symbolic branch scope");
        return false;
    }
    for (size_t i = 0u; i < source->count; ++i) {
        const symbol_t *original = &source->entries[i];
        symbol_t *copy = declare(runtime, target, original->name, original->constant);
        if (!copy)
            return false;
        expr_free(copy->expr);
        copy->expr = original->expr;
        expr_retain(copy->expr);
        copy->leaf = false;
    }
    return true;
}

static value_t execute(runtime_t *runtime, scope_t *scope, char *start, char *end, bool function, size_t depth)
{
    char *p = start;
    if (depth > DEPTH_LIMIT) {
        fail(runtime, "execution nesting limit exceeded");
        return (value_t){0};
    }
    while (p < end && !runtime->failed) {
        space(&p);
        if (p >= end)
            break;
        runtime->position = p;
        if (++runtime->statements > 2048u) {
            fail(runtime, "prototype statement limit exceeded");
            break;
        }
        char *name = identifier(&p);
        if (!name) {
            fail(runtime, "expected a declaration, assignment, return or output");
            break;
        }
        if ((strcmp(name, "expression") == 0 || strcmp(name, "equation") == 0 || strcmp(name, "matrix") == 0) &&
            !function) {
            runtime->function.kind = strcmp(name, "matrix") == 0 ? FUNCTION_MATRIX :
                                     strcmp(name, "equation") == 0 ? FUNCTION_EQUATION : FUNCTION_EXPRESSION;
            free(name);
            define_function(runtime, &p);
            continue;
        }
        if (strcmp(name, "if") == 0 && function) {
            free(name);
            if (!take(&p, '(')) {
                fail(runtime, "expected a parenthesised condition");
                break;
            }
            char *condition = expression_text(runtime, &p);
            if (!condition || !take(&p, ')') || !take(&p, '{')) {
                free(condition);
                fail(runtime, "expected if (condition) { ... }");
                break;
            }
            char *yes = p, *yes_end = matching_block(runtime, p);
            if (!yes_end) {
                free(condition);
                break;
            }
            p = yes_end + 1;
            char *after = p;
            char *otherwise = identifier(&p);
            bool has_else = otherwise && strcmp(otherwise, "else") == 0;
            free(otherwise);
            char *no = NULL, *no_end = NULL;
            if (has_else) {
                if (!take(&p, '{')) {
                    free(condition);
                    fail(runtime, "expected a block after else");
                    break;
                }
                no = p;
                no_end = matching_block(runtime, p);
                if (!no_end) {
                    free(condition);
                    break;
                }
                p = no_end + 1;
            } else {
                p = after;
            }
            expr_t *guard = NULL;
            condition_truth_t truth = condition_value(runtime, scope, condition, 0u, &guard);
            free(condition);
            if (runtime->failed) {
                expr_free(guard);
                break;
            }
            value_t result = {0};
            if (truth == CONDITION_UNKNOWN) {
                if (!guard || !undefined_fallback(no, no_end)) {
                    expr_free(guard);
                    fail(runtime, "condition requires numerical bindings unless it is a supported scalar domain guard");
                    break;
                }
                scope_t local;
                if (branch_scope(runtime, &local, scope))
                    result = execute(runtime, &local, yes, yes_end, true, depth + 1u);
                scope_free(&local);
                if (result.expression && !runtime->failed) {
                    expr_t *guarded = expr_with_domain_of(result.expression, guard);
                    expr_free(result.expression);
                    result.expression = guarded;
                } else {
                    value_free(result);
                    result = (value_t){0};
                    fail(runtime, "symbolic domain guard requires a scalar return value");
                }
            } else if (truth == CONDITION_TRUE || has_else) {
                bool yes_branch = truth == CONDITION_TRUE;
                result = execute(runtime, scope, yes_branch ? yes : no, yes_branch ? yes_end : no_end, true, depth + 1u);
            }
            expr_free(guard);
            if (value_present(result))
                return result;
            continue;
        }
        bool returning = strcmp(name, "return") == 0;
        bool output = strcmp(name, "output") == 0 || strcmp(name, "outputa") == 0;
        bool algebraic = strcmp(name, "outputa") == 0;
        bool constant = strcmp(name, "const") == 0;
        if (constant) {
            free(name);
            name = identifier(&p);
        }
        if (returning && !function)
            fail(runtime, "return is only valid inside a function");
        if (output && function)
            fail(runtime, "output inside a function is not supported yet");
        if (output && !take(&p, '('))
            fail(runtime, "expected output(expression)");
        symbol_t *binding = NULL;
        if (!returning && !output) {
            if (!name || !take(&p, '=')) {
                fail(runtime, "unsupported statement; expected name = expression.");
            } else {
                binding = declare(runtime, scope, name, constant);
            }
        }
        free(name);
        if (runtime->failed)
            break;
        char *text = expression_text(runtime, &p);
        value_t result = {0};
        if (text) {
            if (returning && runtime->function.kind == FUNCTION_MATRIX)
                result.matrix = parse_matrix(runtime, scope, text);
            else
                result = call(runtime, scope, text, 0u);
        }
        expr_t *value = result.expression;
        free(text);
        if (output && !take(&p, ')'))
            fail(runtime, "expected ')' after output argument");
        bool comma = !returning && !output && take(&p, ',');
        if (!comma && !take(&p, '.'))
            fail(runtime, "expected '.' to terminate the statement");
        if (!value_present(result) || runtime->failed) {
            value_free(result);
            break;
        }
        if (returning)
            return result;
        if (output) {
            if (++runtime->outputs > 128u) {
                value_free(result);
                fail(runtime, "prototype output limit exceeded");
                break;
            }
            if (result.matrix) {
                output_matrix(runtime, result.matrix, algebraic);
                value_free(result);
                continue;
            }
            if (result.equation || result.solutions) {
                size_t count = result.equation ? 1u : equ_solutions_count(result.solutions);
                string_t *compact = algebraic && result.solutions
                    ? equ_solutions_compact_text(result.solutions, NULL, style_EXPRESSION) : NULL;
                if (compact)
                    puts(string_c_str(compact));
                for (size_t i = 0u; !compact && i < count; ++i) {
                    const equation_t *equation = result.equation ? result.equation : equ_solutions_at(result.solutions, i);
                    output_equation(runtime, equation, algebraic, result.solutions != NULL);
                }
                string_free(compact);
                if (result.solutions) {
                    if (!count)
                        puts(equ_solutions_proven_empty(result.solutions) ? "No solutions." : "No solution established.");
                    const char *search = equ_solutions_search_note(result.solutions);
                    const char *family = equ_solutions_family_note(result.solutions);
                    if (search) puts(search);
                    if (family) puts(family);
                }
                value_free(result);
                continue;
            }
            expr_t *simplified = expr_beautify(value);
            if (simplified) {
                expr_free(value);
                value = simplified;
            }
            number_t number = expr_eval(value);
            if (!algebraic && !num_is_nan(number)) {
                num_printf("%.*n\n", (int)runtime->precision, number);
            } else {
                char *rendered = expr_to_string(value, style_EXPRESSION);
                if (rendered) {
                    puts(rendered);
                    free(rendered);
                } else {
                    fail(runtime, "could not render symbolic output");
                }
            }
            num_destroy(&number);
        } else if (!value) {
            fail(runtime, "storing matrix, equation or solution-set values is not supported yet; use direct output");
        } else {
            number_t number = expr_eval(value);
            expr_bindings_t *dependencies = NULL;
            char *body = expr_to_function_body(value);
            expr_t *probe = body ? expr_from_function_body(body, &dependencies) : NULL;
            bool numeric_literal = probe && expr_bindings_count(dependencies) == 0u;
            /* Local constant definitions are algebra, not externally supplied numerical bindings. */
            bool exact_local = function && binding->constant && numeric_literal && !num_is_nan(number);
            if (binding->leaf && numeric_literal && !exact_local) {
                expr_set_val(binding->expr, number);
            } else {
                expr_free(binding->expr);
                expr_retain(value);
                binding->expr = value;
                binding->leaf = false;
            }
            expr_free(probe);
            expr_bindings_free(dependencies);
            free(body);
            num_destroy(&number);
        }
        result.expression = value;
        value_free(result);
    }
    return (value_t){0};
}

static bool remove_comments(runtime_t *runtime)
{
    char *p = runtime->source;
    while (*p) {
        char *name_end = bracketed_name_end(p);
        if (name_end) {
            p = name_end;
            continue;
        }
        if (*p != '\x60') {
            ++p;
            continue;
        }
        runtime->position = p;
        *p++ = ' ';
        bool line = *p == '\x60';
        if (line)
            *p++ = ' ';
        while (*p && (line ? *p != '\n' : *p != '\x60')) {
            if (*p != '\n')
                *p = ' ';
            ++p;
        }
        if (!line) {
            if (!*p) {
                fail(runtime, "unterminated comment");
                return false;
            }
            *p++ = ' ';
        }
    }
    return true;
}

/* Read source from standard input; never execute shell commands or source-supplied files. */
/* Execute this built-in calculation mode in a fresh server child process. */
int lab_worker_ophelia(int argc, char **argv)
{
    runtime_t runtime = {0};
    scope_t global;
    runtime.precision = 50u;
    if (argc == 2) {
        char *end;
        unsigned long precision = strtoul(argv[1], &end, 10);
        if (*end || precision < 17u || precision > 10000u) {
            fputs("precision must be between 17 and 10000 decimal digits\n", stderr);
            return 2;
        }
        runtime.precision = (size_t)precision;
    } else if (argc != 1) {
        fputs("usage: ophelia [decimal-digits] < programme\n", stderr);
        return 2;
    }
    runtime.source = calloc(SOURCE_LIMIT + 2u, 1u);
    if (!runtime.source)
        return 2;
    size_t length = fread(runtime.source, 1u, SOURCE_LIMIT + 1u, stdin);
    runtime.position = runtime.source;
    if (length > SOURCE_LIMIT || memchr(runtime.source, '\0', length)) {
        fail(&runtime, "programme exceeds 64 KiB or contains a NUL byte");
        free(runtime.source);
        return 2;
    }
    num_set_default_prec_digits(runtime.precision + 8u);
    if (!scope_init(&global)) {
        free(runtime.source);
        return 2;
    }
    if (remove_comments(&runtime))
        value_free(execute(&runtime, &global, runtime.source, runtime.source + length, false, 0u));
    scope_free(&global);
    free(runtime.function.name);
    for (size_t i = 0; i < runtime.function.count; ++i)
        free(runtime.function.parameters[i]);
    free(runtime.source);
    return runtime.failed ? 1 : 0;
}
