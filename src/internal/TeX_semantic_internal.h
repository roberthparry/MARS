/**
 * @file TeX_semantic_internal.h
 * @brief Internal semantic callbacks, snapshot storage and layout owned by TeX.
 *
 * Mathematical adapters provide semantic nodes without exposing their native
 * structures. Layout helpers format already selected mathematical content.
 * This controlled interface is not part of the public TeX API.
 */
#ifndef MARS_SHARED_TeX_SEMANTIC_INTERNAL_H
#define MARS_SHARED_TeX_SEMANTIC_INTERNAL_H
#if !defined(MARS_SHARED_TeX_SEMANTIC_INTERNAL_ACCESS) && \
    (!defined(__INTELLISENSE__) || !defined(__INCLUDE_LEVEL__) || __INCLUDE_LEVEL__ > 0)
#error "Private header requires MARS_SHARED_TeX_SEMANTIC_INTERNAL_ACCESS."
#endif
#include "TeX-t.h"

/** @brief Opaque binding between a semantic provider and a root node. */
typedef struct _TeX_context_t TeX_context_t;

/** @brief Integral-transform meaning, independent of the native expression representation. */
typedef enum {
    TeX_TRANSFORM_LAPLACE,         /**< Forward Laplace transformation. */
    TeX_TRANSFORM_INVERSE_LAPLACE, /**< Inverse Laplace transformation. */
    TeX_TRANSFORM_FOURIER,         /**< Forward Fourier transformation. */
    TeX_TRANSFORM_INVERSE_FOURIER  /**< Inverse Fourier transformation. */
} TeX_transform_kind_t;

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
    TeX_RELATION_IMPLIES      /**< Logical implication. */
} TeX_relation_t;

/** @brief Standard number sets, supplied as mathematical meaning rather than commands. */
typedef enum {
    TeX_SET_REALS,                /**< Real numbers. */
    TeX_SET_COMPLEX,              /**< Complex numbers. */
    TeX_SET_INTEGERS,             /**< Integers. */
    TeX_SET_NONNEGATIVE_INTEGERS,  /**< Integers greater than or equal to zero. */
    TeX_SET_NATURALS              /**< Natural numbers. */
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

/** @brief Forward declaration of a provider's read-only callback table. */
typedef struct _TeX_provider_t TeX_provider_t;

/**
 * @brief Borrowed node and its interpretation context.
 *
 * TeX never dereferences node or context. Both and the provider table must remain
 * valid throughout rendering. Children may use different providers.
 */
typedef struct {
    const TeX_provider_t *provider;
    const void *context;
    const void *node;
} TeX_node_ref_t;

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
    TeX_node_kind_t kind;
    size_t child_count;
    const string_t *text;
    size_t rows;
    size_t columns;
    unsigned order;
    bool partial;
    bool domain_qualification; /**< The final domain child is a separately scoped distribution qualification. */
    TeX_transform_kind_t transform; /**< Transform meaning, used only by TRANSFORM. */
    TeX_relation_t relation; /**< Relation meaning, used only by RELATION. */
    TeX_number_set_t number_set; /**< Standard set, used only by NUMBER_SET. */
    TeX_fence_t fence; /**< Delimiter choice, used only by FENCED. */
} TeX_node_view_t;

/**
 * @brief Callbacks exposing semantic structure without transferring ownership.
 *
 * Return false on failure. Describe must fill a complete view; child supplies
 * the indexed child without modifying the graph. Callbacks must not return TeX
 * syntax, free borrowed nodes or retain TeX's output pointers. No callback is
 * invoked concurrently within one rendering call. Re-entry is permitted.
 */
struct _TeX_provider_t {
    bool (*describe)(const void *context, const void *node, TeX_node_view_t *view);
    bool (*child)(const void *context, const void *node, size_t index, TeX_node_ref_t *child);
};

/**
 * @brief Bind a borrowed semantic root to a new owned rendering context.
 * @return Owned context, released with TeX_context_free, or NULL for invalid arguments or allocation failure.
 * @note The referenced graph, provider tables and provider contexts must outlive every use of this context.
 */
TeX_context_t *TeX_context_new(TeX_node_ref_t root);

/**
 * @brief Render semantic callbacks to an independently owned TeX object.
 * @return Owned notation, released with TeX_free, or NULL on invalid data, cycles or allocation failure.
 * @note Each call has independent traversal state. Concurrent calls require thread-safe providers.
 * @note Cycles, paths exceeding 256 nodes and nodes with over 65,536 children are rejected.
 * The context is unchanged on failure.
 */
TeX_t *TeX_from_context(const TeX_context_t *context);

/**
 * @brief Free a context and any snapshot it owns; NULL is accepted.
 * @note Provider tables and graphs borrowed by TeX_context_new are never freed.
 */
void TeX_context_free(TeX_context_t *context);


/** @brief Adopt root.context on success only; release it once when the context is freed. */
TeX_context_t *TeX_context_adopt(TeX_node_ref_t root, void (*release)(const void *));

/** @brief Borrow the root reference; it remains valid only while its context and graph remain alive. */
TeX_node_ref_t TeX_context_root(const TeX_context_t *context);

/** @brief Owned semantic-node storage; release with TeX_model_free until transferred to a context. */
typedef struct _TeX_model_t TeX_model_t;

/** @brief Opaque semantic node owned by its model. */
typedef struct _TeX_model_node_t TeX_model_node_t;

/** @brief Allocate an empty semantic model. */
TeX_model_t *TeX_model_new(void);

/** @brief Copy a node's view, text and child references into model-owned storage; return a borrowed node. */
const TeX_model_node_t *TeX_model_add(TeX_model_t *model, TeX_node_view_t view,
                                      const TeX_model_node_t *const *children);

/** @brief Transfer model ownership into a context on success only; root must belong to the model. */
TeX_context_t *TeX_model_context(TeX_model_t *model, const TeX_model_node_t *root);

/** @brief Release all model nodes and copied text; NULL is accepted. */
void TeX_model_free(TeX_model_t *model);


/** @brief Presentation of a mathematical qualification. */
typedef enum {
    TeX_QUALIFICATION_INLINE, /**< An underbrace caption attached to the operand. */
    TeX_QUALIFICATION_CONDITION, /**< A caption following a condition separator. */
    TeX_QUALIFICATION_TRAILING, /**< A parenthesised caption after the expression. */
    TeX_QUALIFICATION_FINITE_PART /**< A finite-part operator around the operand. */
} TeX_qualification_style_t;

/** @brief Format an operand and plain-text qualification without interpreting their meaning. */
string_t *TeX_qualification(const string_t *operand, const char *caption, TeX_qualification_style_t style);

/** @brief Look up a recognised mathematical text literal; return immutable notation or NULL. */
const char *TeX_known_math_text(const char *text);

/** @brief Convert Unicode mathematical notation into a newly allocated TeX string. */
char *TeX_from_unicode_math(const char *text);

/** @brief Return owned absolute notation for a leading negative coefficient, or NULL when not negative. */
string_t *TeX_negative_coefficient_abs(const string_t *text);

/** @brief Append a recognised telescoping-product identity with a borrowed endpoint. */
int TeX_append_telescoping_product(string_t *out, const char *endpoint);

/** @brief Append the positive or negative reciprocal-prime product identity. */
int TeX_append_prime_product(string_t *out, bool positive);

/** @brief Append a finite trigonometric sum using a plain mathematical function name. */
int TeX_append_trigonometric_sum(string_t *out, const char *endpoint, const char *function,
                                 const char *angle, bool weighted);

/** @brief Align rendered definitions and equation rows; return a new owned string. */
string_t *TeX_align_equation_rows(const string_t *definitions, const string_t *left,
                                  const string_t *const *right, size_t count);

/** @brief Align a starting expression and optional result, with an optional two-case result. */
string_t *TeX_align_derivation(const string_t *start, const char *result, const char *alternative,
                               const char *condition, const char *condition_value);

/** @brief Enclose mathematical notation in compact automatically sized parentheses. */
string_t *TeX_parenthesise_compact(const string_t *body);
#endif
