#include "expr_stringout.h"
#define MARS_EXPR_STRINGOUT_INTERNAL_ACCESS
#include "expr_stringout_internal.h"

/* Match one complete, explicitly grouped occurrence in the rendered body. */
const char *expr_distribution_group(const char *body, const char *operand);

static _Thread_local expr_distribution_TeX_scope_t *distribution_scope;
static _Thread_local const expr_t *expression_detached;

const char *expr_distribution_qualification(const expr_t *expr)
{
    if (expr_is_half_line_finite_part(expr))
        return NULL;
    if (expr_is_op(expr, &ops_principal_value))
        return "principal value";
    if (expr_is_op(expr, &ops_finite_part))
        return "finite part";
    return NULL;
}

/* Count occurrences, including shared DAG edges: distinct occurrences need distinct scopes. */
static void distribution_find(const expr_t *expr, const expr_t **found, size_t *count)
{
    if (!expr || *count > 1u)
        return;
    if (expr_distribution_qualification(expr)) {
        *found = expr;
        ++*count;
    }
    distribution_find(expr->a, found, count);
    distribution_find(expr->b, found, count);
}

static size_t distribution_operand_occurrences(const expr_t *expr, const expr_t *operand)
{
    if (!expr)
        return 0u;
    if (expr_simplify_same_factor(expr, operand))
        return 1u;
    size_t left = distribution_operand_occurrences(expr->a, operand);
    return left > 1u ? left : left + distribution_operand_occurrences(expr->b, operand);
}

bool expr_distribution_has_qualification(const expr_t *expr)
{
    const expr_t *found = NULL;
    size_t count = 0u;
    distribution_find(expr, &found, &count);
    return count != 0u;
}

static const expr_t *distribution_detachable(const expr_t *root)
{
    const expr_t *found = NULL;
    size_t count = 0u;
    distribution_find(root, &found, &count);
    return count == 1u && distribution_operand_occurrences(root, found->a) == 1u ? found : NULL;
}

const expr_t *expr_distribution_expr_body(const expr_t *root, sbuf_t *buffer)
{
    const expr_t *saved = expression_detached;
    const expr_t *detached = distribution_detachable(root);
    expression_detached = detached;
    const expr_t *body = expr_is_op(root, &ops_real_domain) ? root->a : root;
    sbuf_t rendered;
    sbuf_init(&rendered);
    emit_expr(body, &rendered, PREC_LOWEST);
    if (detached) {
        sbuf_t operand;
        sbuf_init(&operand);
        emit_expr(detached->a, &operand, PREC_LOWEST);
        if (!expr_distribution_group(sbuf_c_str(&rendered), sbuf_c_str(&operand))) {
            /* Display reordering may introduce an equal-looking group. Retain local scope in that case. */
            expression_detached = detached = NULL;
            sbuf_free(&rendered);
            sbuf_init(&rendered);
            emit_expr(body, &rendered, PREC_LOWEST);
        }
        sbuf_free(&operand);
    }
    sbuf_puts(buffer, sbuf_c_str(&rendered));
    sbuf_free(&rendered);
    expression_detached = saved;
    return detached;
}

bool expr_distribution_expr_emit(const expr_t *expr, sbuf_t *buffer)
{
    if (expr_is_half_line_finite_part(expr)) {
        sbuf_putc(buffer, '(');
        emit_expr(expr->a, buffer, PREC_LOWEST);
        sbuf_putc(buffer, ')');
        return true;
    }
    const char *qualification = expr_distribution_qualification(expr);
    if (!qualification)
        return false;
    sbuf_putc(buffer, '(');
    emit_expr(expr->a, buffer, PREC_LOWEST);
    if (expr != expression_detached) {
        sbuf_puts(buffer, " : ");
        sbuf_puts(buffer, qualification);
    }
    sbuf_putc(buffer, ')');
    return true;
}

void expr_distribution_expr_caption(const expr_t *expr, sbuf_t *buffer)
{
    emit_expr(expr->a, buffer, PREC_LOWEST);
    sbuf_puts(buffer, " : ");
    sbuf_puts(buffer, expr_distribution_qualification(expr));
}

void expr_distribution_TeX_begin(const expr_t *root, expr_distribution_TeX_scope_t *scope)
{
    scope->root = root;
    scope->detached = distribution_detachable(root);
    scope->appended = false;
    scope->outer = distribution_scope;
    distribution_scope = scope;
}

static void distribution_TeX_caption(expr_distribution_TeX_scope_t *scope, sbuf_t *buffer)
{
    /* The argument has no nested qualification when a caption can be detached. */
    emit_TeX_expr(scope->detached->a, buffer, PREC_LOWEST);
    sbuf_puts(buffer, ":\\;\\text{");
    sbuf_puts(buffer, expr_distribution_qualification(scope->detached));
    sbuf_putc(buffer, '}');
    scope->appended = true;
}

void expr_distribution_TeX_conditions(const expr_t *root, sbuf_t *buffer)
{
    if (distribution_scope && distribution_scope->root == root && distribution_scope->detached) {
        sbuf_puts(buffer, ";\\;");
        distribution_TeX_caption(distribution_scope, buffer);
    }
}

void expr_distribution_TeX_end(expr_distribution_TeX_scope_t *scope, sbuf_t *buffer)
{
    if (scope->detached && !scope->appended) {
        sbuf_puts(buffer, "\\quad (");
        distribution_TeX_caption(scope, buffer);
        sbuf_putc(buffer, ')');
    }
    distribution_scope = scope->outer;
}

bool expr_distribution_TeX_emit(const expr_t *expr, sbuf_t *buffer, int parent_prec)
{
    if (expr_is_half_line_finite_part(expr)) {
        emit_TeX_expr(expr->a, buffer, parent_prec);
        return true;
    }
    const char *qualification = expr_distribution_qualification(expr);
    if (!qualification)
        return false;
    if (distribution_scope && distribution_scope->detached == expr) {
        emit_TeX_expr(expr->a, buffer, parent_prec);
    } else {
        /* Braces delimit precisely this operator, including nesting and unqualified duplicates. */
        sbuf_puts(buffer, "\\underbrace{");
        emit_TeX_expr(expr->a, buffer, parent_prec);
        sbuf_puts(buffer, "}_{\\text{");
        sbuf_puts(buffer, qualification);
        sbuf_puts(buffer, "}}");
    }
    return true;
}
