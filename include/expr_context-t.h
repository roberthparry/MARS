/**
 * @file expr_context-t.h
 * @brief Owned assumptions and binding snapshots for symbolic expressions.
 *
 * expr_context_t separates a symbol's role, mathematical assumptions and current
 * numerical binding from the expression graph. It supplies binding-aware display
 * simplification and numerical evaluation without changing the original formula.
 * Structural proof queries let native solvers distinguish established facts from
 * unknown properties without treating free-variable samples as assumptions.
 * Variables are identified by their expression identity (preserved by cloning),
 * and named constants by their canonical name within one context namespace.
 *
 * Contexts own their data; neither symbols nor input numbers are borrowed after
 * a setter returns. Results are independent snapshots, not live views. Recompute
 * from the original formula after changing a context, not from a previous result
 * whose restrictions may already have been discharged. Contexts and expression
 * graphs require external synchronisation; this API does not change the process
 * number precision or impose a solver work budget.
 */
#ifndef MARS_EXPR_CONTEXT_H
#define MARS_EXPR_CONTEXT_H

#include <stdbool.h>
#include <stdint.h>

#include "expression-t.h"

/** @brief Opaque owner of symbol roles, assumptions and numerical bindings. */
typedef struct _expr_context_t expr_context_t;

/** @brief Whether a current value may justify a binding-specific display rule. */
typedef enum {
    EXPR_CONTEXT_FREE,      /**< Retain the symbol; only explicit assumptions justify rewriting. */
    EXPR_CONTEXT_PARAMETER /**< Retain the symbol; its current binding may also justify rewriting. */
} expr_context_role_t;

/** @brief Combinable assumptions; positive and negative imply real and non-zero. */
typedef enum {
    EXPR_CONTEXT_REAL = 1u,
    EXPR_CONTEXT_NONZERO = 2u,
    EXPR_CONTEXT_POSITIVE = 4u,
    EXPR_CONTEXT_NEGATIVE = 8u
} expr_context_assumption_t;

/** @brief Create an empty context, or return NULL with errno on allocation failure. */
expr_context_t *expr_context_new(void);

/**
 * @brief Capture current symbol bindings outside unresolved calculus scopes.
 * @param source Borrowed expression; neither it nor its binding objects are retained.
 * @param wrt Active variable to keep free; NULL keeps all variables free. Other variables become parameters.
 * @return Owned context, or NULL with errno. Named constants become parameters; no assumptions are inferred.
 * Infinite bindings are omitted. Inconsistent copies of one symbol fail with EDOM rather than selecting a value.
 */
expr_context_t *expr_context_from_expression(const expr_t *source, const expr_t *wrt);

/** @brief Deep-copy a context, including its revision, or return NULL with errno. */
expr_context_t *expr_context_clone(const expr_context_t *context);

/** @brief Release a context and its owned bindings; NULL is harmless. */
void expr_context_free(expr_context_t *context);

/**
 * @brief Atomically replace one symbol's role, assumptions and numerical binding.
 * @param context Required context.
 * @param symbol Variable or mutable named constant; built-in constants and compound expressions are rejected.
 * @param role Free symbol or parameter. This does not change the expression's differentiation semantics.
 * @param assumptions Bitwise combination of expr_context_assumption_t, or zero for none.
 * @param value Borrowed number to copy; NUM_NAN represents an unset binding. Other values must be finite.
 * @return Success; invalid arguments use EINVAL, contradictory assumptions/bindings use EDOM.
 * Failed updates leave the context and revision unchanged. Each successful update advances the revision.
 */
bool expr_context_set(expr_context_t *context, const expr_t *symbol, expr_context_role_t role,
                      unsigned assumptions, number_t value);

/**
 * @brief Look up a symbol without scanning the binding collection.
 * @param context Required context.
 * @param symbol Symbol or a clone retaining its identity.
 * @param role Optional role output.
 * @param assumptions Optional normalised assumption output; numerical values do not become assumptions.
 * @param value Optional output receiving an owned number; destroy it with num_destroy().
 * @return Whether found; outputs remain unchanged on failure. An absent symbol is not an error.
 */
bool expr_context_get(const expr_context_t *context, const expr_t *symbol, expr_context_role_t *role,
                      unsigned *assumptions, number_t *value);

/** @brief Remove a symbol and advance the revision; return false if absent or invalid. */
bool expr_context_remove(expr_context_t *context, const expr_t *symbol);

/** @brief Return the mutation revision, initially zero; NULL returns zero. */
uint64_t expr_context_revision(const expr_context_t *context);

/**
 * @brief Produce an owned symbolic display snapshot using this context's evidence.
 *
 * Keeps symbol names, extracts known signed real squares from principal roots and
 * discharges proven real-domain conditions. Known false conditions give NAN;
 * unproved conditions remain. Unregistered symbols retain their source bindings
 * but cannot supply numerical proof. Explicit assumptions also apply to free
 * variables; their current values never justify display specialisation.
 * Unresolved calculus scopes are left untouched. NULL arguments return NULL.
 */
expr_t *expr_context_simplify(const expr_context_t *context, const expr_t *source);

/**
 * @brief Refine an already arranged result only when context evidence changes its algebra.
 * @param context Borrowed symbol policy and binding snapshot.
 * @param source Borrowed result graph; never modified.
 * @return Owned expression, or NULL for invalid input or failure; release with expr_free().
 * Applies the same sign and domain proofs as expr_context_simplify(), but skips general
 * beautification when none applies, preserving compact solver forms. Bindings are still copied.
 */
expr_t *expr_context_refine(const expr_context_t *context, const expr_t *source);

/**
 * @brief Test whether structural evidence establishes every requested assumption bit.
 * @param context Borrowed context; required.
 * @param source Borrowed expression; required.
 * @param assumptions Non-empty combination of EXPR_CONTEXT_REAL, NONZERO, POSITIVE and NEGATIVE.
 * @return True only when all requested facts follow; false also covers unknown or invalid input.
 * Uses explicit assumptions, supplied parameter values and structural arithmetic rules.
 * Free-variable samples and approximate evaluation of compound expressions are never proof.
 * False does not prove the opposite. Neither input nor global precision is changed.
 */
bool expr_context_proves(const expr_context_t *context, const expr_t *source, unsigned assumptions);

/**
 * @brief Evaluate a fresh binding snapshot, returning an owned number (NAN on invalid input).
 *
 * Free symbols and parameters both use their supplied values. Missing context
 * entries retain source bindings. Evaluation delegates to expr_eval(); this is
 * not a solver, and unresolved calculus scopes are not rebound.
 */
number_t expr_context_evaluate(const expr_context_t *context, const expr_t *source);

#endif
