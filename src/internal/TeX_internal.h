/**
 * @file TeX_internal.h
 * @brief Shared semantic snapshots and layout owned by TeX.
 *
 * Mathematical adapters provide semantic nodes without exposing their native
 * structures. Layout helpers format already selected mathematical content.
 * Native mathematical modules and authorised white-box tests consume this guarded interface. Define
 * MARS_SHARED_TeX_INTERNAL_ACCESS immediately before inclusion. Ordinary callers use TeX-t.h or their
 * mathematical module's output API; rendering contexts are never required by those public APIs.
 * Owned notation is released with TeX_free, contexts with TeX_context_free and models with TeX_model_free.
 * Callback data marked borrowed remains caller-owned and must outlive the rendering call.
 */
#ifndef MARS_SHARED_TeX_INTERNAL_H
#define MARS_SHARED_TeX_INTERNAL_H
#if !defined(MARS_SHARED_TeX_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_TeX_INTERNAL_ACCESS."
#endif
#include "TeX-t.h"

/** @brief Opaque binding between a semantic provider and a root node. */
typedef struct _TeX_context_t TeX_context_t;

/** @brief Mathematical relations supplied without typesetting commands. */
typedef enum {
    TeX_RELATION_EQUAL,       /**< Equality. */
    TeX_RELATION_NOT_EQUAL,   /**< Inequality. */
    TeX_RELATION_LESS,        /**< Strictly less than. */
    TeX_RELATION_LESS_EQUAL,  /**< Less than or equal to. */
    TeX_RELATION_GREATER,     /**< Strictly greater than. */
    TeX_RELATION_GREATER_EQUAL, /**< Greater than or equal to. */
    TeX_RELATION_MEMBER,      /**< Membership of a set or region. */
    TeX_RELATION_DEFINITION,  /**< Defining equality. */
    TeX_RELATION_IMPLIES,     /**< Logical implication. */
    TeX_RELATION_APPROXIMATE, /**< Approximate equality, without a numerical tolerance. */
    TeX_RELATION_CONGRUENT    /**< Congruence, without testing geometric or algebraic equivalence. */
} TeX_relation_t;

/** @brief Standard number sets, supplied as mathematical meaning rather than commands. */
typedef enum {
    TeX_SET_REALS,                /**< Real numbers. */
    TeX_SET_COMPLEX,              /**< Complex numbers. */
    TeX_SET_INTEGERS,             /**< Integers. */
    TeX_SET_NONNEGATIVE_INTEGERS,  /**< Integers greater than or equal to zero. */
    TeX_SET_NATURALS,             /**< Natural numbers. */
    TeX_SET_PRIMES                /**< Prime numbers. */
} TeX_number_set_t;

/** @brief Mathematical structure supplied by a provider, independent of native object layouts. */
typedef enum {
    TeX_NODE_NUMBER,      /**< Plain decimal or scientific numeric text; no children. */
    TeX_NODE_IDENTIFIER,  /**< Plain mathematical name; no children. */
    TeX_NODE_TEXT,        /**< Literal prose; no children. */
    TeX_NODE_SUM,         /**< Left and right summands. */
    TeX_NODE_DIFFERENCE,  /**< Minuend and subtrahend. */
    TeX_NODE_PRODUCT,     /**< Left and right factors. */
    TeX_NODE_FRACTION,    /**< Numerator and denominator. */
    TeX_NODE_POWER,       /**< Base and exponent. */
    TeX_NODE_ROOT,        /**< Radicand and optional root index. */
    TeX_NODE_NEGATIVE,    /**< One operand to negate. */
    TeX_NODE_EQUATION,    /**< Left and right sides of an equality. */
    TeX_NODE_MATRIX,      /**< Row-major entries, with rows and columns supplied. */
    TeX_NODE_CALL,        /**< Plain function name and ordered arguments. */
    TeX_NODE_DERIVATIVE,  /**< Body, ordered variables, order and ordinary/partial style. */
    TeX_NODE_INTEGRAL,    /**< Body, variable, optional upper bound alone or lower and upper bounds. */
    TeX_NODE_SEQUENCE,    /**< Consecutive presentation items. */
    TeX_NODE_SUBSCRIPT,   /**< Base and subscript. */
    TeX_NODE_EVALUATION,  /**< Body followed by variable-assignment equalities. */
    TeX_NODE_LINES,       /**< Successive mathematical or prose rows. */
    TeX_NODE_TRANSFORM,   /**< Body, source coordinate and target coordinate. */
    TeX_NODE_RELATION,    /**< Two operands and their mathematical relation. */
    TeX_NODE_DOMAIN_RELATION, /**< Two operands and a relation within a domain restriction. */
    TeX_NODE_DOMAIN,      /**< An expression followed by its domain restrictions. */
    TeX_NODE_DISTRIBUTION_QUALIFICATION, /**< Body and optional distinct operand with a plain-text qualification. */
    TeX_NODE_DISTRIBUTION_INLINE, /**< Locally scoped distribution operand and plain-text qualification. */
    TeX_NODE_DISTRIBUTION_CONDITION, /**< Operand and plain-text qualification inside a domain condition. */
    TeX_NODE_FINITE_PART, /**< An operand interpreted as a half-line finite-part distribution. */
    TeX_NODE_STATEMENTS,  /**< Consecutive statements separated by mathematical space. */
    TeX_NODE_CASES,       /**< Alternating values and their conditions. */
    TeX_NODE_SUMMATION,   /**< Body, index, lower bound and upper bound. */
    TeX_NODE_INDEXED_PRODUCT, /**< Body, index, lower bound and upper bound. */
    TeX_NODE_CONDITIONAL_PRODUCT, /**< Body, index condition and optionally an upper bound. */
    TeX_NODE_NUMBER_SET,  /**< A standard number set; no children. */
    TeX_NODE_LIST,        /**< Ordered, comma-separated mathematical items. */
    TeX_NODE_FENCED,      /**< One child and an explicit fence style. */
    TeX_NODE_BOUND,       /**< Body and optional list of bindings or conditions. */
    TeX_NODE_BINDINGS,    /**< Variable assignments followed by constant assignments; rows gives the variable count. */
    TeX_NODE_SCALED,      /**< Scalar factor and mathematical object. */
    TeX_NODE_CARTESIAN,   /**< Real and imaginary components. */
    TeX_NODE_COMPLEX_LITERAL, /**< Real and imaginary parts of a complex numeric literal. */
    TeX_NODE_IMAGINARY_DISPLACEMENT, /**< Symbolic coefficient of the imaginary unit in a complex shift. */
    TeX_NODE_ANTIDERIVATIVE_DEFINITION, /**< Integration variable and integrand of the chosen antiderivative F. */
    TeX_NODE_DIFFERENTIAL, /**< Variable of an ordinary or partial differential. */
    TeX_NODE_FUNCTION_NAME, /**< Plain canonical or user-defined function name; no children. */
    TeX_NODE_APPLY,       /**< Function notation followed by its ordered arguments. */
    TeX_NODE_ORDERED_DERIVATIVE, /**< Function name, derivative order and function arguments. */
    TeX_NODE_VARIABLE_DERIVATIVE, /**< Dependent variable, ordered coordinates and derivative order. */
    TeX_NODE_CONDITIONS,  /**< Governing statement followed by its accompanying conditions. */
    TeX_NODE_PARAMETER_GROUPS, /**< Ordered groups of function parameters, separated by semicolons. */
    TeX_NODE_LEFT_SUBSCRIPT, /**< Base and an index placed before it. */
    TeX_NODE_ASSIGNMENT,  /**< A variable and its assigned evaluation value. */
    TeX_NODE_BINDING_ASSIGNMENT, /**< A symbol and its value in an expression binding list. */
    TeX_NODE_HAT,         /**< One mathematical object marked with a wide hat. */
    TeX_NODE_PRIME,       /**< One object with a positive derivative order in prime notation. */
    TeX_NODE_BOLD,        /**< One mathematical symbol or expression in bold. */
    TeX_NODE_CALLIGRAPHIC, /**< One mathematical symbol in calligraphic style. */
    TeX_NODE_INTEGRAL_CONDITION, /**< Integrand, integration condition and measure. */
    TeX_NODE_FACTORIAL,   /**< One operand to which the factorial operation is applied. */
    TeX_NODE_REAL_PART,   /**< The real part of one mathematical operand. */
    TeX_NODE_CONJUGATE,   /**< The complex conjugate of one mathematical operand. */
    TeX_NODE_REAL_PROJECTION, /**< A grouped real-part projection. */
    TeX_NODE_IMAG_PROJECTION, /**< A grouped imaginary-part projection. */
    TeX_NODE_FUNCTION_ORDER, /**< Function symbol, mathematical order and argument. */
    TeX_NODE_COUNT        /**< Array bound, not a valid node kind. */
} TeX_node_kind_t;

/**
 * @brief Semantic properties of a node, copied before its children are requested.
 *
 * Text is plain UTF-8, never TeX source, and is borrowed until the next callback.
 * NUMBER accepts decimal/scientific notation; IDENTIFIER and CALL carry names;
 * TEXT carries literal prose. MATRIX supplies row-major children and dimensions.
 * DERIVATIVE supplies a body and ordered variable children, a positive order and its style.
 * ROOT has one or two children (radicand, optional index); INTEGRAL has body,
 * variable and optionally an upper bound alone or both bounds. SUBSCRIPT supplies base and index.
 * EVALUATION supplies a body and one or more variable-assignment children.
 * PRIME supplies one object and a positive derivative order for prime notation.
 * BOLD and CALLIGRAPHIC each supply one mathematical object. INTEGRAL_CONDITION
 * supplies an integrand, an integration condition and a differential measure.
 * DOMAIN contains a body followed by conditions; domain_qualification marks its
 * final child as a separately scoped distribution qualification. A distribution
 * condition supplies one operand and a plain-text caption.
 * LINES supplies successive mathematical or prose rows. With one derivative variable,
 * order may exceed one; with several variables, order must equal their count.
 * Other arities are checked by TeX.
 */
typedef struct {
    TeX_node_kind_t kind; /**< Mathematical node kind selecting a TeX-owned renderer. */
    size_t child_count; /**< Number of ordered children available through the provider. */
    const string_t *text; /**< Borrowed plain UTF-8 text, copied before the next provider callback. */
    size_t rows; /**< Matrix row count, or variable-assignment count for BINDINGS. */
    size_t columns; /**< Matrix column count; zero when unused. */
    unsigned order; /**< Derivative or prime order; zero when unused. */
    bool partial; /**< Select partial rather than ordinary derivative or differential notation. */
    bool domain_qualification; /**< The final domain child is a separately scoped distribution qualification. */
    TeX_transform_kind_t transform; /**< Transform meaning, used only by TRANSFORM. */
    TeX_relation_t relation; /**< Relation meaning, used only by RELATION. */
    TeX_number_set_t number_set; /**< Standard set, used only by NUMBER_SET. */
    TeX_fence_t fence; /**< Delimiter choice, used only by FENCED. */
} TeX_node_view_t;

/**
 * @brief Render semantic callbacks to an independently owned TeX object.
 * @param context Borrowed rendering context whose provider state and graph remain valid throughout the call.
 * @return Owned notation, released with TeX_free, or NULL on invalid data, cycles or allocation failure.
 * @note Each call has independent traversal state. Concurrent calls require thread-safe providers.
 * @note Cycles, paths exceeding 256 nodes and nodes with over 65,536 children are rejected.
 * The context is unchanged on failure.
 */
TeX_t *TeX_from_context(const TeX_context_t *context);

/**
 * @brief Native-owned selection of the mathematical terms available for multiline display.
 * @details Callbacks run sequentially during TeX_wrap_sum. The caller owns state and keeps it alive throughout
 * the call. Count may prepare a selection; term returns an owned context which TeX renders and frees, and sets
 * sign to 1 or -1. TeX does not inspect the callback state or the caller's native mathematical objects.
 */
typedef struct _TeX_sum_provider_t {
    /**
     * @brief Prepare the native term selection only when TeX requests a multiline alternative.
     * @param state Borrowed caller-owned selection state, valid throughout TeX_wrap_sum.
     * @return Number of selected terms; zero or one retains the ordinary body without requesting terms.
     */
    size_t (*count)(void *state);

    /**
     * @brief Supply one independently owned semantic term context in the selected display order.
     * @param state Borrowed caller-owned state previously passed to count.
     * @param index Zero-based index, strictly below the count returned for this selection.
     * @param sign Required output receiving 1 or -1 for the additive sign of this term.
     * @return Owned context which TeX renders and frees with TeX_context_free, or NULL to retain the ordinary body.
     * @note The context must own its snapshot or keep its borrowed graph valid until TeX has rendered it.
     */
    TeX_context_t *(*term)(void *state, size_t index, int *sign);

} TeX_sum_provider_t;

/**
 * @brief Render a semantic body, using native-selected terms when it exceeds a soft line budget.
 * @param context Borrowed ordinary body context.
 * @param line_limit Soft TeX source-line budget; zero selects 110 bytes.
 * @param provider Borrowed optional term provider; queried only when wrapping is necessary.
 * @param state Borrowed caller-owned state passed unchanged to the callbacks.
 * @return Owned notation released with TeX_free, or NULL when the ordinary body cannot be rendered.
 * @note If a multiline alternative is unavailable, the valid ordinary body is retained.
 */
TeX_t *TeX_wrap_sum(const TeX_context_t *context, size_t line_limit,
                    const TeX_sum_provider_t *provider, void *state);

/** @brief Mathematical component requested from a scalar-matrix provider. */
typedef enum {
    TeX_SCALAR_WHOLE,     /**< The complete scalar entry. */
    TeX_SCALAR_REAL,      /**< Its real component. */
    TeX_SCALAR_IMAGINARY  /**< Its signed imaginary coefficient, without the imaginary unit. */
} TeX_scalar_component_t;

/**
 * @brief Caller-owned scalar-matrix interpretation callbacks.
 * @details Entry returns an owned context which TeX renders and frees. Index is row-major. The callback sets
 * has_imaginary when the native entry has a non-zero imaginary component. Calls are sequential; provider and
 * state remain caller-owned and must stay valid throughout TeX_scalar_matrix.
 */
typedef struct _TeX_scalar_matrix_provider_t {
    /**
     * @brief Supply an owned component context and report the native entry's imaginary-part presence.
     * @param state Borrowed callback state passed unchanged to the supplied provider.
     * @param index Zero-based position of the requested operand, coordinate or term.
     * @param component Mathematical scalar component to snapshot: whole value, real part or signed imaginary
     * coefficient.
     * @param has_imaginary Required caller-owned output reporting whether the native scalar has a non-zero imaginary
     * component.
     * @return Owned rendering context released with TeX_context_free, or NULL on invalid input or failure.
     */
    TeX_context_t *(*entry)(void *state, size_t index, TeX_scalar_component_t component, bool *has_imaginary);
} TeX_scalar_matrix_provider_t;

/**
 * @brief Render scalar entries, separating long complex matrices into real and imaginary matrices when useful.
 * @param rows Positive number of matrix rows; rows times columns may not exceed 65,536.
 * @param columns Positive number of matrix columns; rows times columns may not exceed 65,536.
 * @param provider Borrowed provider callbacks; the caller retains the table and its state throughout rendering.
 * @param state Borrowed callback state passed unchanged to the supplied provider.
 * @return Owned notation released with TeX_free, or NULL for invalid dimensions or unavailable scalar entries.
 * @note Rows and columns must be positive and their product may not exceed 65,536 entries.
 */
TeX_t *TeX_scalar_matrix(size_t rows, size_t columns, const TeX_scalar_matrix_provider_t *provider, void *state);

/**
 * @brief Free a context and any snapshot it owns; NULL is accepted.
 * @param context Owned context to release; NULL is accepted.
 * @note Snapshot storage owned by the context is released; externally owned provider data is not freed.
 */
void TeX_context_free(TeX_context_t *context);

/** @brief Owned semantic-node storage; release with TeX_model_free until transferred to a context. */
typedef struct _TeX_model_t TeX_model_t;

/** @brief Opaque semantic node owned by its model. */
typedef struct _TeX_model_node_t TeX_model_node_t;

/**
 * @brief Allocate an owned empty model released with TeX_model_free; return NULL on allocation failure.
 * @return Owned model released with TeX_model_free, or NULL on allocation failure.
 */
TeX_model_t *TeX_model_new(void);

/**
 * @brief Copy a descriptor and its text into model-owned semantic storage.
 * @param model Borrowed mutable model; every supplied child must already belong to this model.
 * @param view Descriptor copied by value; optional text is cloned and may be freed after the call.
 * @param children Borrowed array of view.child_count child pointers; NULL is allowed for no children.
 * @return Model-owned node, or NULL on failure. Free the model rather than individual nodes.
 * @note Child arrays are copied. Mathematical validity is checked during rendering.
 */
const TeX_model_node_t *TeX_model_add(TeX_model_t *model, TeX_node_view_t view,
                                      const TeX_model_node_t *const *children);

/**
 * @brief Transfer a model and its root into an independently owned rendering context.
 * @param model Owned model to transfer on success; the caller retains ownership on failure.
 * @param root Borrowed root belonging to model.
 * @return Owned context released with TeX_context_free, or NULL on invalid input or allocation failure.
 * @note After success, neither mutate nor separately free the model or its nodes.
 */
TeX_context_t *TeX_model_context(TeX_model_t *model, const TeX_model_node_t *root);

/**
 * @brief Release all model nodes and copied text; NULL is accepted.
 * @param model Owned model to release with all its nodes; NULL is accepted.
 */
void TeX_model_free(TeX_model_t *model);

/**
 * @brief Align rendered definitions and equation rows; return a new owned string.
 * @param definitions Borrowed rendered definitions to precede the equation rows; the string remains caller-owned.
 * @param left Borrowed mathematical operand; no input ownership transfers.
 * @param right Borrowed mathematical operand; no input ownership transfers.
 * @param count Number of entries in the supplied ordered array or collection.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *TeX_align_equation_rows(const string_t *definitions, const string_t *left,
                                  const string_t *const *right, size_t count);

/**
 * @brief Format ordered mathematical definitions; return an independently owned string.
 * @param names Borrowed ordered array of symbol names, corresponding to the supplied values or native symbols.
 * @param values Borrowed count-element array of independently owned rendered notation values; entries are not
 * consumed.
 * @param count Number of entries in the supplied ordered array or collection.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *TeX_format_definitions(const string_t *const *names, const TeX_t *const *values, size_t count);

/**
 * @brief Align a starting expression and optional result, with an optional two-case result.
 * @param start Borrowed rendered initial mathematical expression for the derivation.
 * @param result Borrowed already computed mathematical result or rendered result text, according to its declared
 * type.
 * @param alternative Borrowed rendered limiting-case result, or NULL when no alternative case is requested.
 * @param condition Borrowed rendered variable or expression used to label the alternative case.
 * @param condition_value Borrowed rendered exceptional value selecting the alternative case.
 * @return Owned text released with string_free, or NULL when no representation is available or allocation fails.
 */
string_t *TeX_align_derivation(const string_t *start, const char *result, const char *alternative,
                               const char *condition, const char *condition_value);

#endif
