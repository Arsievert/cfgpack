#ifndef CFGPACK_ERROR_H
#define CFGPACK_ERROR_H

/**
 * @file error.h
 * @brief Error codes for cfgpack APIs.
 */

/**
 * @brief Error codes returned by cfgpack APIs.
 */
typedef enum {
    CFGPACK_OK = 0,                   /**< Success. */
    CFGPACK_ERR_PARSE = -1,           /**< Parse failure. */
    CFGPACK_ERR_INVALID_TYPE = -2,    /**< Unknown or unsupported type. */
    CFGPACK_ERR_DUPLICATE = -3,       /**< Duplicate index or name. */
    CFGPACK_ERR_BOUNDS = -4,          /**< Out of bounds (index, length, count). */
    CFGPACK_ERR_MISSING = -5,         /**< Entry or value not present. */
    CFGPACK_ERR_TYPE_MISMATCH = -6,   /**< Value type does not match schema. */
    CFGPACK_ERR_STR_TOO_LONG = -7,    /**< String exceeds fixed limits. */
    CFGPACK_ERR_IO = -8,              /**< I/O failure. */
    CFGPACK_ERR_ENCODE = -9,          /**< Encoding failure or output too small. */
    CFGPACK_ERR_DECODE = -10,         /**< Decoding failure or malformed input. */
    CFGPACK_ERR_RESERVED_INDEX = -11  /**< Attempt to use reserved index 0. */
} cfgpack_err_t;

#endif /* CFGPACK_ERROR_H */
