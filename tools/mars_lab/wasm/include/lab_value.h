/**
 * @file lab_value.h
 * @brief Browser value categories shared by the DOM bridge and binary codec.
 *
 * These fixed tags cross the WebAssembly import boundary. They classify values,
 * not handles: DOM and codec handles still belong to separate scoped tables.
 * The codec rejects unsupported values with the negative invalid tag.
 */
#ifndef LAB_WASM_VALUE_H
#define LAB_WASM_VALUE_H

/** @brief Stable browser value tags; numeric values match the host bridge. */
typedef enum {
    LAB_VALUE_INVALID = -1, /**< Unsupported value or invalid codec handle. */
    LAB_VALUE_NULL = 0,     /**< Null or undefined. */
    LAB_VALUE_BOOLEAN = 1,  /**< Boolean scalar. */
    LAB_VALUE_NUMBER = 2,   /**< Binary64 numeric scalar. */
    LAB_VALUE_STRING = 3,   /**< Browser string. */
    LAB_VALUE_ARRAY = 4,    /**< Indexed array. */
    LAB_VALUE_OBJECT = 5,  /**< Object record; the codec requires a supported plain record. */
    LAB_VALUE_BYTES = 6    /**< Opaque byte array; never interpreted as text. */
} lab_value_kind_t;

#endif
