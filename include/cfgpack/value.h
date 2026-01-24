#ifndef CFGPACK_VALUE_H
#define CFGPACK_VALUE_H

/**
 * @file value.h
 * @brief Value types and containers for cfgpack.
 *
 * Defines the supported data types and the tagged union container
 * used to store configuration values.
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
 * @brief Value container tagged by @c type.
 */
typedef struct {
    cfgpack_type_t type;
    union {
        uint64_t u64;
        int64_t i64;
        float   f32;
        double  f64;
        struct { uint16_t len; char data[CFGPACK_STR_MAX + 1]; } str;
        struct { uint8_t  len; char data[CFGPACK_FSTR_MAX + 1]; } fstr;
    } v;
} cfgpack_value_t;

#endif /* CFGPACK_VALUE_H */
