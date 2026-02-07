#ifndef CFGPACK_VALUE_H
#define CFGPACK_VALUE_H

/**
 * @file value.h
 * @brief Value types and containers for cfgpack.
 *
 * Defines the supported data types and the tagged union containers
 * used to store configuration values.
 *
 * Two value types are provided:
 * - cfgpack_value_t: Compact (16 bytes), strings stored in external pool
 * - cfgpack_fat_value_t: Inline strings (~72 bytes), used for parsing defaults
 */

#include <stdint.h>

/**
 * @brief Supported cfgpack value types.
 */
typedef enum {
    CFGPACK_TYPE_U8,
    CFGPACK_TYPE_U16,
    CFGPACK_TYPE_U32,
    CFGPACK_TYPE_U64,
    CFGPACK_TYPE_I8,
    CFGPACK_TYPE_I16,
    CFGPACK_TYPE_I32,
    CFGPACK_TYPE_I64,
    CFGPACK_TYPE_F32,
    CFGPACK_TYPE_F64,
    CFGPACK_TYPE_STR,
    CFGPACK_TYPE_FSTR
} cfgpack_type_t;

/** Maximum variable string length (bytes). */
#define CFGPACK_STR_MAX 64
/** Maximum fixed string length (bytes). */
#define CFGPACK_FSTR_MAX 16

/**
 * @brief Compact value container tagged by @c type.
 *
 * String data is stored in an external pool; the str/fstr members
 * contain an offset into the pool and the current length.
 */
typedef struct {
    cfgpack_type_t type;
    union {
        uint64_t u64;
        int64_t  i64;
        float    f32;
        double   f64;
        struct { uint16_t offset; uint16_t len; } str;
        struct { uint16_t offset; uint8_t len; uint8_t _pad; } fstr;
    } v;
} cfgpack_value_t;

/**
 * @brief Fat value container with inline string storage.
 *
 * Used for parsing schema defaults. Strings are stored inline,
 * making this type ~72 bytes. Use cfgpack_value_t for runtime storage.
 */
typedef struct {
    cfgpack_type_t type;
    union {
        uint64_t u64;
        int64_t  i64;
        float    f32;
        double   f64;
        struct { uint16_t len; char data[CFGPACK_STR_MAX + 1]; } str;
        struct { uint8_t  len; char data[CFGPACK_FSTR_MAX + 1]; } fstr;
    } v;
} cfgpack_fat_value_t;

#endif /* CFGPACK_VALUE_H */
