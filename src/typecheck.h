#ifndef CFGPACK_TYPECHECK_H
#define CFGPACK_TYPECHECK_H

/**
 * @file typecheck.h
 * @brief Shared range check for integer values against their declared type.
 *
 * A cfgpack_value_t stores every integer in a 64-bit union member, so the
 * type tag alone does not guarantee the number fits the schema width.  Every
 * path that stores a caller- or wire-supplied integer (cfgpack_set, pagein
 * coercion, schema default parsing) must apply this check so that pageout
 * never emits a wider wire format than the schema permits.
 */

#include "cfgpack/value.h"

#include <stdint.h>

/**
 * @brief Test whether an integer value fits its declared type.
 *
 * Non-integer types trivially fit.
 *
 * @param type Declared schema type.
 * @param v    Value whose u64/i64 member is checked according to @p type.
 * @return 1 if the value is within range for @p type, 0 otherwise.
 */
static inline int cfgpack_int_fits(cfgpack_type_t type,
                                   const cfgpack_value_t *v) {
    switch (type) {
    case CFGPACK_TYPE_U8: return (v->v.u64 <= UINT8_MAX);
    case CFGPACK_TYPE_U16: return (v->v.u64 <= UINT16_MAX);
    case CFGPACK_TYPE_U32: return (v->v.u64 <= UINT32_MAX);
    case CFGPACK_TYPE_I8: return (v->v.i64 >= INT8_MIN && v->v.i64 <= INT8_MAX);
    case CFGPACK_TYPE_I16:
        return (v->v.i64 >= INT16_MIN && v->v.i64 <= INT16_MAX);
    case CFGPACK_TYPE_I32:
        return (v->v.i64 >= INT32_MIN && v->v.i64 <= INT32_MAX);
    default: return (1);
    }
}

#endif /* CFGPACK_TYPECHECK_H */
