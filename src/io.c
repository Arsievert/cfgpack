#include "cfgpack/api.h"
#include "cfgpack/msgpack.h"
#include "cfgpack/error.h"
#include "cfgpack/value.h"
#include "cfgpack/schema.h"

#include <stdio.h>
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
    if (cfgpack_msgpack_encode_map_header(&buf, (uint32_t)present_count) != CFGPACK_OK) return CFGPACK_ERR_ENCODE;

    for (size_t i = 0; i < ctx->schema->entry_count; ++i) {
        if (!cfgpack_presence_get(ctx, i)) continue;
        const cfgpack_entry_t *e = &ctx->schema->entries[i];
        if (cfgpack_msgpack_encode_uint_key(&buf, e->index) != CFGPACK_OK) return CFGPACK_ERR_ENCODE;
        if (encode_value(&buf, &ctx->values[i]) != CFGPACK_OK) return CFGPACK_ERR_ENCODE;
    }

    if (out_len) *out_len = buf.len;
    return (buf.len <= out_cap) ? CFGPACK_OK : CFGPACK_ERR_ENCODE;
}

cfgpack_err_t cfgpack_pageout_file(const cfgpack_ctx_t *ctx, const char *path, uint8_t *scratch, size_t scratch_cap) {
    size_t len = 0;
    FILE *f;
    cfgpack_err_t rc;

    rc = cfgpack_pageout(ctx, scratch, scratch_cap, &len);
    if (rc != CFGPACK_OK) return rc;

    f = fopen(path, "wb");
    if (!f) return CFGPACK_ERR_IO;
    if (fwrite(scratch, 1, len, f) != len) {
        fclose(f);
        return CFGPACK_ERR_IO;
    }
    fclose(f);
    return CFGPACK_OK;
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

cfgpack_err_t cfgpack_pagein_buf(cfgpack_ctx_t *ctx, const uint8_t *data, size_t len) {
    cfgpack_reader_t r;
    uint32_t map_count = 0;

    if (!data || len == 0) return CFGPACK_ERR_DECODE;
    cfgpack_reader_init(&r, data, len);
    if (cfgpack_msgpack_decode_map_header(&r, &map_count) != CFGPACK_OK) return CFGPACK_ERR_DECODE;

    memset(ctx->present, 0, ctx->present_bytes);

    for (uint32_t i = 0; i < map_count; ++i) {
        uint64_t key;
        if (cfgpack_msgpack_decode_uint64(&r, &key) != CFGPACK_OK) return CFGPACK_ERR_DECODE;
        /* find matching entry */
        const cfgpack_entry_t *entry = NULL;
        size_t idx = 0;
        for (size_t j = 0; j < ctx->schema->entry_count; ++j) {
            if (ctx->schema->entries[j].index == (uint16_t)key) {
                entry = &ctx->schema->entries[j];
                idx = j;
                break;
            }
        }
        if (!entry) return CFGPACK_ERR_DECODE;
        if (decode_value(&r, entry->type, &ctx->values[idx]) != CFGPACK_OK) return CFGPACK_ERR_DECODE;
        cfgpack_presence_set(ctx, idx);
    }
    return CFGPACK_OK;
}

cfgpack_err_t cfgpack_pagein_file(cfgpack_ctx_t *ctx, const char *path, uint8_t *scratch, size_t scratch_cap) {
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) return CFGPACK_ERR_IO;
    n = fread(scratch, 1, scratch_cap, f);
    if (n == scratch_cap && !feof(f)) {
        fclose(f);
        return CFGPACK_ERR_IO; /* file too big for scratch */
    }
    fclose(f);
    return cfgpack_pagein_buf(ctx, scratch, n);
}
