#include "cfgpack/api.h"
#include "cfgpack/msgpack.h"
#include "cfgpack/error.h"
#include "cfgpack/value.h"
#include "cfgpack/schema.h"

#include <string.h>

#define PAGE_CAP 4096

/**
 * @brief Encode a cfgpack value into msgpack format.
 * @param buf  Output buffer.
 * @param v    Value to encode.
 * @return CFGPACK_OK on success, CFGPACK_ERR_ENCODE or CFGPACK_ERR_INVALID_TYPE on failure.
 */
static cfgpack_err_t encode_value(cfgpack_buf_t *buf, const cfgpack_value_t *v) {
    switch (v->type) {
        case CFGPACK_TYPE_U8: return cfgpack_msgpack_encode_uint64(buf, v->v.u64);
        case CFGPACK_TYPE_U16: return cfgpack_msgpack_encode_uint64(buf, v->v.u64);
        case CFGPACK_TYPE_U32: return cfgpack_msgpack_encode_uint64(buf, v->v.u64);
        case CFGPACK_TYPE_U64: return cfgpack_msgpack_encode_uint64(buf, v->v.u64);
        case CFGPACK_TYPE_I8: return cfgpack_msgpack_encode_int64(buf, v->v.i64);
        case CFGPACK_TYPE_I16: return cfgpack_msgpack_encode_int64(buf, v->v.i64);
        case CFGPACK_TYPE_I32: return cfgpack_msgpack_encode_int64(buf, v->v.i64);
        case CFGPACK_TYPE_I64: return cfgpack_msgpack_encode_int64(buf, v->v.i64);
        case CFGPACK_TYPE_F32: return cfgpack_msgpack_encode_f32(buf, v->v.f32);
        case CFGPACK_TYPE_F64: return cfgpack_msgpack_encode_f64(buf, v->v.f64);
        case CFGPACK_TYPE_STR: return cfgpack_msgpack_encode_str(buf, v->v.str.data, v->v.str.len);
        case CFGPACK_TYPE_FSTR: return cfgpack_msgpack_encode_str(buf, v->v.fstr.data, v->v.fstr.len);
    }
    return CFGPACK_ERR_INVALID_TYPE;
}

cfgpack_err_t cfgpack_pageout(const cfgpack_ctx_t *ctx, uint8_t *out, size_t out_cap, size_t *out_len) {
    cfgpack_buf_t buf;
    size_t present_count = 0;

    /* Minimum headroom: map header + u64 key + u8 value (~12 bytes). */
    if (out_cap < 12) return CFGPACK_ERR_ENCODE;

    for (size_t i = 0; i < ctx->schema->entry_count; ++i) {
        if (cfgpack_presence_get(ctx, i)) present_count++;
    }

    cfgpack_buf_init(&buf, out, out_cap);
    /* Map header includes +1 for reserved index 0 (schema name) */
    if (cfgpack_msgpack_encode_map_header(&buf, (uint32_t)(present_count + 1)) != CFGPACK_OK) return CFGPACK_ERR_ENCODE;

    /* Write schema name at reserved index 0 */
    if (cfgpack_msgpack_encode_uint_key(&buf, CFGPACK_INDEX_RESERVED_NAME) != CFGPACK_OK) return CFGPACK_ERR_ENCODE;
    if (cfgpack_msgpack_encode_str(&buf, ctx->schema->map_name, strlen(ctx->schema->map_name)) != CFGPACK_OK) return CFGPACK_ERR_ENCODE;

    for (size_t i = 0; i < ctx->schema->entry_count; ++i) {
        if (!cfgpack_presence_get(ctx, i)) continue;
        const cfgpack_entry_t *e = &ctx->schema->entries[i];
        if (cfgpack_msgpack_encode_uint_key(&buf, e->index) != CFGPACK_OK) return CFGPACK_ERR_ENCODE;
        if (encode_value(&buf, &ctx->values[i]) != CFGPACK_OK) return CFGPACK_ERR_ENCODE;
    }

    if (out_len) *out_len = buf.len;
    return (buf.len <= out_cap) ? CFGPACK_OK : CFGPACK_ERR_ENCODE;
}

cfgpack_err_t cfgpack_peek_name(const uint8_t *data, size_t len, char *out_name, size_t out_cap) {
    cfgpack_reader_t r;
    uint32_t map_count;

    if (!data || len == 0 || !out_name || out_cap == 0) return CFGPACK_ERR_DECODE;

    cfgpack_reader_init(&r, data, len);
    if (cfgpack_msgpack_decode_map_header(&r, &map_count) != CFGPACK_OK) return CFGPACK_ERR_DECODE;

    /* Scan map for key 0 (reserved schema name) */
    for (uint32_t i = 0; i < map_count; ++i) {
        uint64_t key;
        if (cfgpack_msgpack_decode_uint64(&r, &key) != CFGPACK_OK) return CFGPACK_ERR_DECODE;

        if (key == CFGPACK_INDEX_RESERVED_NAME) {
            /* Found key 0, expect string value */
            const uint8_t *str_ptr;
            uint32_t str_len;
            if (cfgpack_msgpack_decode_str(&r, &str_ptr, &str_len) != CFGPACK_OK) return CFGPACK_ERR_DECODE;

            /* Check if name fits in output buffer (need space for null terminator) */
            if (str_len + 1 > out_cap) return CFGPACK_ERR_BOUNDS;

            memcpy(out_name, str_ptr, str_len);
            out_name[str_len] = '\0';
            return CFGPACK_OK;
        }

        /* Skip this value and continue searching */
        if (cfgpack_msgpack_skip_value(&r) != CFGPACK_OK) return CFGPACK_ERR_DECODE;
    }

    /* Key 0 not found */
    return CFGPACK_ERR_MISSING;
}

/**
 * @brief Decode a msgpack value into a cfgpack value.
 * @param r     Reader state.
 * @param type  Expected type to decode.
 * @param out   Output value structure.
 * @return CFGPACK_OK on success, error code on failure.
 */
static cfgpack_err_t decode_value(cfgpack_reader_t *r, cfgpack_type_t type, cfgpack_value_t *out) {
    out->type = type;
    switch (type) {
        case CFGPACK_TYPE_U8:
        case CFGPACK_TYPE_U16:
        case CFGPACK_TYPE_U32:
        case CFGPACK_TYPE_U64:
            return cfgpack_msgpack_decode_uint64(r, &out->v.u64);
        case CFGPACK_TYPE_I8:
        case CFGPACK_TYPE_I16:
        case CFGPACK_TYPE_I32:
        case CFGPACK_TYPE_I64:
            return cfgpack_msgpack_decode_int64(r, &out->v.i64);
        case CFGPACK_TYPE_F32:
            return cfgpack_msgpack_decode_f32(r, &out->v.f32);
        case CFGPACK_TYPE_F64:
            return cfgpack_msgpack_decode_f64(r, &out->v.f64);
        case CFGPACK_TYPE_STR: {
            const uint8_t *ptr; uint32_t len;
            if (cfgpack_msgpack_decode_str(r, &ptr, &len) != CFGPACK_OK) return CFGPACK_ERR_DECODE;
            if (len > CFGPACK_STR_MAX) return CFGPACK_ERR_STR_TOO_LONG;
            out->v.str.len = (uint16_t)len;
            memcpy(out->v.str.data, ptr, len);
            out->v.str.data[len] = '\0';
            return CFGPACK_OK;
        }
        case CFGPACK_TYPE_FSTR: {
            const uint8_t *ptr; uint32_t len;
            if (cfgpack_msgpack_decode_str(r, &ptr, &len) != CFGPACK_OK) return CFGPACK_ERR_DECODE;
            if (len > CFGPACK_FSTR_MAX) return CFGPACK_ERR_STR_TOO_LONG;
            out->v.fstr.len = (uint8_t)len;
            memcpy(out->v.fstr.data, ptr, len);
            out->v.fstr.data[len] = '\0';
            return CFGPACK_OK;
        }
    }
    return CFGPACK_ERR_INVALID_TYPE;
}

/**
 * @brief Check if type coercion (widening) is allowed from wire type to schema type.
 *
 * Allowed widening conversions:
 * - u8 -> u16/u32/u64
 * - u16 -> u32/u64
 * - u32 -> u64
 * - i8 -> i16/i32/i64
 * - i16 -> i32/i64
 * - i32 -> i64
 * - f32 -> f64
 * - fstr -> str (if length fits)
 *
 * Additionally, small unsigned integers can be read as signed types since
 * MessagePack encodes small non-negative values as positive fixint/uint:
 * - u8 -> i8/i16/i32/i64 (value must fit, checked at decode time)
 * - u16 -> i16/i32/i64
 * - u32 -> i32/i64
 * - u64 -> i64
 *
 * @param wire_type   Type detected from msgpack wire format.
 * @param schema_type Type declared in schema.
 * @return 1 if coercion is allowed (or exact match), 0 otherwise.
 */
static int can_coerce_type(cfgpack_type_t wire_type, cfgpack_type_t schema_type) {
    if (wire_type == schema_type) return 1;

    /* Unsigned widening */
    if (wire_type == CFGPACK_TYPE_U8) {
        if (schema_type == CFGPACK_TYPE_U16 || schema_type == CFGPACK_TYPE_U32 || schema_type == CFGPACK_TYPE_U64)
            return 1;
        /* Small unsigned -> signed (value range checked at decode) */
        if (schema_type == CFGPACK_TYPE_I8 || schema_type == CFGPACK_TYPE_I16 || 
            schema_type == CFGPACK_TYPE_I32 || schema_type == CFGPACK_TYPE_I64)
            return 1;
    }
    if (wire_type == CFGPACK_TYPE_U16) {
        if (schema_type == CFGPACK_TYPE_U32 || schema_type == CFGPACK_TYPE_U64)
            return 1;
        /* u16 -> signed (value range checked at decode) */
        if (schema_type == CFGPACK_TYPE_I16 || schema_type == CFGPACK_TYPE_I32 || schema_type == CFGPACK_TYPE_I64)
            return 1;
    }
    if (wire_type == CFGPACK_TYPE_U32) {
        if (schema_type == CFGPACK_TYPE_U64)
            return 1;
        /* u32 -> signed (value range checked at decode) */
        if (schema_type == CFGPACK_TYPE_I32 || schema_type == CFGPACK_TYPE_I64)
            return 1;
    }
    if (wire_type == CFGPACK_TYPE_U64) {
        /* u64 -> i64 (value range checked at decode) */
        if (schema_type == CFGPACK_TYPE_I64)
            return 1;
    }

    /* Signed widening */
    if (wire_type == CFGPACK_TYPE_I8) {
        return schema_type == CFGPACK_TYPE_I16 || schema_type == CFGPACK_TYPE_I32 || schema_type == CFGPACK_TYPE_I64;
    }
    if (wire_type == CFGPACK_TYPE_I16) {
        return schema_type == CFGPACK_TYPE_I32 || schema_type == CFGPACK_TYPE_I64;
    }
    if (wire_type == CFGPACK_TYPE_I32) {
        return schema_type == CFGPACK_TYPE_I64;
    }

    /* Float widening */
    if (wire_type == CFGPACK_TYPE_F32) {
        return schema_type == CFGPACK_TYPE_F64;
    }

    /* String widening: fstr -> str */
    if (wire_type == CFGPACK_TYPE_FSTR) {
        return schema_type == CFGPACK_TYPE_STR;
    }

    return 0;
}

/**
 * @brief Detect the type of the next msgpack value and decode it.
 *
 * Peeks at the format byte to determine the wire type, then decodes
 * the value accordingly. Supports type coercion during decode.
 *
 * @param r           Reader state.
 * @param schema_type Expected schema type (for coercion check).
 * @param out         Output value structure.
 * @return CFGPACK_OK on success, CFGPACK_ERR_TYPE_MISMATCH if incompatible,
 *         CFGPACK_ERR_DECODE on parse error.
 */
static cfgpack_err_t decode_value_with_coercion(cfgpack_reader_t *r, cfgpack_type_t schema_type, cfgpack_value_t *out) {
    if (r->pos >= r->len) return CFGPACK_ERR_DECODE;

    uint8_t b = r->data[r->pos];
    cfgpack_type_t wire_type;

    /* Detect wire type from format byte (don't consume yet) */
    if (b <= 0x7f) {
        /* Positive fixint: treat as smallest unsigned that fits */
        wire_type = CFGPACK_TYPE_U8;
    } else if (b >= 0xe0) {
        /* Negative fixint: treat as smallest signed that fits */
        wire_type = CFGPACK_TYPE_I8;
    } else if (b == 0xcc) {
        wire_type = CFGPACK_TYPE_U8;
    } else if (b == 0xcd) {
        wire_type = CFGPACK_TYPE_U16;
    } else if (b == 0xce) {
        wire_type = CFGPACK_TYPE_U32;
    } else if (b == 0xcf) {
        wire_type = CFGPACK_TYPE_U64;
    } else if (b == 0xd0) {
        wire_type = CFGPACK_TYPE_I8;
    } else if (b == 0xd1) {
        wire_type = CFGPACK_TYPE_I16;
    } else if (b == 0xd2) {
        wire_type = CFGPACK_TYPE_I32;
    } else if (b == 0xd3) {
        wire_type = CFGPACK_TYPE_I64;
    } else if (b == 0xca) {
        wire_type = CFGPACK_TYPE_F32;
    } else if (b == 0xcb) {
        wire_type = CFGPACK_TYPE_F64;
    } else if ((b & 0xe0) == 0xa0 || b == 0xd9 || b == 0xda) {
        /* String: peek at length to decide fstr vs str */
        /* For simplicity, we'll detect based on what we're coercing to */
        if (schema_type == CFGPACK_TYPE_FSTR || schema_type == CFGPACK_TYPE_STR) {
            wire_type = schema_type; /* Match schema expectation for strings */
        } else {
            return CFGPACK_ERR_TYPE_MISMATCH;
        }
    } else {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }

    /* Check if coercion is allowed */
    if (!can_coerce_type(wire_type, schema_type)) {
        return CFGPACK_ERR_TYPE_MISMATCH;
    }

    /* Decode the value using the schema type */
    return decode_value(r, schema_type, out);
}

cfgpack_err_t cfgpack_pagein_remap(cfgpack_ctx_t *ctx, const uint8_t *data, size_t len,
                                    const cfgpack_remap_entry_t *remap, size_t remap_count) {
    cfgpack_reader_t r;
    uint32_t map_count = 0;

    if (!data || len == 0) return CFGPACK_ERR_DECODE;
    cfgpack_reader_init(&r, data, len);
    if (cfgpack_msgpack_decode_map_header(&r, &map_count) != CFGPACK_OK) return CFGPACK_ERR_DECODE;

    memset(ctx->present, 0, ctx->present_bytes);

    for (uint32_t i = 0; i < map_count; ++i) {
        uint64_t key;
        if (cfgpack_msgpack_decode_uint64(&r, &key) != CFGPACK_OK) return CFGPACK_ERR_DECODE;

        /* Skip reserved index 0 (schema name) */
        if (key == CFGPACK_INDEX_RESERVED_NAME) {
            if (cfgpack_msgpack_skip_value(&r) != CFGPACK_OK) return CFGPACK_ERR_DECODE;
            continue;
        }

        /* Apply remap if provided */
        uint16_t target_index = (uint16_t)key;
        if (remap != NULL) {
            for (size_t ri = 0; ri < remap_count; ++ri) {
                if (remap[ri].old_index == (uint16_t)key) {
                    target_index = remap[ri].new_index;
                    break;
                }
            }
        }

        /* Find matching entry in schema */
        const cfgpack_entry_t *entry = NULL;
        size_t idx = 0;
        for (size_t j = 0; j < ctx->schema->entry_count; ++j) {
            if (ctx->schema->entries[j].index == target_index) {
                entry = &ctx->schema->entries[j];
                idx = j;
                break;
            }
        }

        /* Unknown key: silently skip */
        if (!entry) {
            if (cfgpack_msgpack_skip_value(&r) != CFGPACK_OK) return CFGPACK_ERR_DECODE;
            continue;
        }

        /* Decode value with type coercion support */
        cfgpack_err_t err = decode_value_with_coercion(&r, entry->type, &ctx->values[idx]);
        if (err != CFGPACK_OK) return err;
        cfgpack_presence_set(ctx, idx);
    }
    return CFGPACK_OK;
}

cfgpack_err_t cfgpack_pagein_buf(cfgpack_ctx_t *ctx, const uint8_t *data, size_t len) {
    return cfgpack_pagein_remap(ctx, data, len, NULL, 0);
}
