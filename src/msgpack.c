#include "cfgpack/msgpack.h"

#include "cfgpack/config.h"

#include <string.h>

void cfgpack_buf_init(cfgpack_buf_t *buf, uint8_t *storage, size_t cap) {
    buf->data = storage;
    buf->cap = cap;
    buf->len = 0;
}

/* Measure semantics, matching wbuf_try_append: on overflow nothing is
 * written but len still advances, so a caller can learn the required size
 * from a buffer that was too small. */
cfgpack_err_t cfgpack_buf_append(cfgpack_buf_t *buf,
                                 const void *src,
                                 size_t len) {
    if (buf->len + len > buf->cap) {
        buf->len += len;
        return (CFGPACK_ERR_ENCODE);
    }
    if (len > 0) {
        memcpy(buf->data + buf->len, src, len);
    }
    buf->len += len;
    return (CFGPACK_OK);
}

cfgpack_err_t cfgpack_msgpack_encode_uint64(cfgpack_buf_t *buf, uint64_t v) {
    uint8_t tmp[9];
    size_t n = 0;
    if (v <= 0x7fu) {
        tmp[n++] = (uint8_t)v;
    } else if (v <= 0xffu) {
        tmp[n++] = 0xcc;
        tmp[n++] = (uint8_t)v;
    } else if (v <= 0xffffu) {
        tmp[n++] = 0xcd;
        tmp[n++] = (uint8_t)(v >> 8);
        tmp[n++] = (uint8_t)v;
    } else if (v <= 0xffffffffu) {
        tmp[n++] = 0xce;
        tmp[n++] = (uint8_t)(v >> 24);
        tmp[n++] = (uint8_t)(v >> 16);
        tmp[n++] = (uint8_t)(v >> 8);
        tmp[n++] = (uint8_t)v;
    } else {
        tmp[n++] = 0xcf;
        for (int i = 7; i >= 0; --i) {
            tmp[n++] = (uint8_t)(v >> (8 * i));
        }
    }
    return (cfgpack_buf_append(buf, tmp, n));
}

cfgpack_err_t cfgpack_msgpack_encode_int64(cfgpack_buf_t *buf, int64_t v) {
    uint8_t tmp[9];
    size_t n = 0;

    if (v >= 0) {
        return (cfgpack_msgpack_encode_uint64(buf, (uint64_t)v));
    }
    if (v >= -32) {
        tmp[n++] = (uint8_t)v; /* negative fixint */
    } else if (v >= -128) {
        tmp[n++] = 0xd0;
        tmp[n++] = (uint8_t)v;
    } else if (v >= -32768) {
        tmp[n++] = 0xd1;
        tmp[n++] = (uint8_t)(v >> 8);
        tmp[n++] = (uint8_t)v;
    } else if (v >= INT32_MIN) {
        tmp[n++] = 0xd2;
        tmp[n++] = (uint8_t)(v >> 24);
        tmp[n++] = (uint8_t)(v >> 16);
        tmp[n++] = (uint8_t)(v >> 8);
        tmp[n++] = (uint8_t)v;
    } else {
        tmp[n++] = 0xd3;
        for (int i = 7; i >= 0; --i) {
            tmp[n++] = (uint8_t)(v >> (8 * i));
        }
    }
    return (cfgpack_buf_append(buf, tmp, n));
}

cfgpack_err_t cfgpack_msgpack_encode_f32(cfgpack_buf_t *buf, float v) {
    uint8_t tmp[5];
    uint32_t u;

    memcpy(&u, &v, sizeof(u));
    tmp[0] = 0xca;
    tmp[1] = (uint8_t)(u >> 24);
    tmp[2] = (uint8_t)(u >> 16);
    tmp[3] = (uint8_t)(u >> 8);
    tmp[4] = (uint8_t)u;
    return (cfgpack_buf_append(buf, tmp, sizeof(tmp)));
}

cfgpack_err_t cfgpack_msgpack_encode_f64(cfgpack_buf_t *buf, double v) {
    uint8_t tmp[9];
    uint64_t u;

    memcpy(&u, &v, sizeof(u));
    tmp[0] = 0xcb;
    for (int i = 0; i < 8; ++i) {
        tmp[1 + i] = (uint8_t)(u >> (56 - 8 * i));
    }
    return (cfgpack_buf_append(buf, tmp, sizeof(tmp)));
}

cfgpack_err_t cfgpack_msgpack_encode_str(cfgpack_buf_t *buf,
                                         const char *s,
                                         size_t len) {
    size_t hlen = 0;
    cfgpack_err_t rc;
    uint8_t hdr[3];
    if (len > 0xffffu) {
        /* str16 is the largest header emitted; a longer string would get a
         * wrapped length header followed by all len bytes (corrupt output). */
        return (CFGPACK_ERR_ENCODE);
    }
    if (len <= 31) {
        hdr[hlen++] = (uint8_t)(0xa0 | (uint8_t)len);
    } else if (len <= 255) {
        hdr[hlen++] = 0xd9;
        hdr[hlen++] = (uint8_t)len;
    } else {
        hdr[hlen++] = 0xda;
        hdr[hlen++] = (uint8_t)(len >> 8);
        hdr[hlen++] = (uint8_t)len;
    }
    rc = cfgpack_buf_append(buf, hdr, hlen);
    if (cfgpack_buf_append(buf, s, len) != CFGPACK_OK) {
        rc = CFGPACK_ERR_ENCODE;
    }
    return (rc);
}

cfgpack_err_t cfgpack_msgpack_encode_map_header(cfgpack_buf_t *buf,
                                                uint32_t count) {
    uint8_t tmp[3];
    size_t n = 0;
    if (count > 0xffffu) {
        /* map16 is the largest header emitted */
        return (CFGPACK_ERR_ENCODE);
    }
    if (count <= 15) {
        tmp[n++] = (uint8_t)(0x80 | count);
    } else {
        tmp[n++] = 0xde;
        tmp[n++] = (uint8_t)(count >> 8);
        tmp[n++] = (uint8_t)count;
    }
    return (cfgpack_buf_append(buf, tmp, n));
}

cfgpack_err_t cfgpack_msgpack_encode_uint_key(cfgpack_buf_t *buf, uint64_t v) {
    return (cfgpack_msgpack_encode_uint64(buf, v));
}

cfgpack_err_t cfgpack_msgpack_encode_str_key(cfgpack_buf_t *buf,
                                             const char *s,
                                             size_t len) {
    return (cfgpack_msgpack_encode_str(buf, s, len));
}

void cfgpack_reader_init(cfgpack_reader_t *r, const uint8_t *data, size_t len) {
    r->data = data;
    r->len = len;
    r->pos = 0;
}

/**
 * @brief Read bytes from a msgpack reader.
 * @param r    Reader state.
 * @param dst  Destination buffer.
 * @param n    Number of bytes to read.
 * @return 0 on success, -1 if insufficient data.
 */
static int read_bytes(cfgpack_reader_t *r, void *dst, size_t n) {
    if (r->pos + n > r->len) {
        return (-1);
    }
    memcpy(dst, r->data + r->pos, n);
    r->pos += n;
    return (0);
}

cfgpack_err_t cfgpack_msgpack_decode_map_header(cfgpack_reader_t *r,
                                                uint32_t *count) {
    uint8_t b;
    if (read_bytes(r, &b, 1)) {
        return (CFGPACK_ERR_DECODE);
    }
    if ((b & 0xf0u) == 0x80u) {
        *count = (uint32_t)(b & 0x0f);
        return (CFGPACK_OK);
    }
    if (b == 0xde) {
        uint8_t bytes[2];
        if (read_bytes(r, bytes, 2)) {
            return (CFGPACK_ERR_DECODE);
        }
        *count = ((uint32_t)bytes[0] << 8) | bytes[1];
        return (CFGPACK_OK);
    }
    return (CFGPACK_ERR_DECODE);
}

/**
 * @brief Decode the payload of a uint 8/16/32/64 value whose format byte
 *        @p b has already been consumed.
 * @return CFGPACK_OK; CFGPACK_ERR_DECODE if @p b is not a uint format or
 *         the payload is truncated.
 */
static cfgpack_err_t decode_uint_payload(cfgpack_reader_t *r,
                                         uint8_t b,
                                         uint64_t *out) {
    uint64_t res = 0;
    uint8_t v[8];
    size_t n;

    switch (b) {
    case 0xcc: n = 1; break;
    case 0xcd: n = 2; break;
    case 0xce: n = 4; break;
    case 0xcf: n = 8; break;
    default: return (CFGPACK_ERR_DECODE);
    }
    if (read_bytes(r, v, n)) {
        return (CFGPACK_ERR_DECODE);
    }
    for (size_t i = 0; i < n; ++i) {
        res = (res << 8) | v[i];
    }
    *out = res;
    return (CFGPACK_OK);
}

cfgpack_err_t cfgpack_msgpack_decode_uint64(cfgpack_reader_t *r,
                                            uint64_t *out) {
    uint8_t b;
    if (read_bytes(r, &b, 1)) {
        return (CFGPACK_ERR_DECODE);
    }
    if (b <= 0x7f) {
        *out = b;
        return (CFGPACK_OK);
    }
    return (decode_uint_payload(r, b, out));
}

cfgpack_err_t cfgpack_msgpack_decode_int64(cfgpack_reader_t *r, int64_t *out) {
    uint64_t u;
    uint8_t b;
    if (read_bytes(r, &b, 1)) {
        return (CFGPACK_ERR_DECODE);
    }
    if (b <= 0x7f) {
        *out = (int64_t)b;
        return (CFGPACK_OK);
    }
    if (b >= 0xe0) {
        *out = (int8_t)b;
        return (CFGPACK_OK);
    }
    if (b == 0xd0) {
        int8_t v;
        if (read_bytes(r, &v, 1)) {
            return (CFGPACK_ERR_DECODE);
        }
        *out = v;
        return (CFGPACK_OK);
    }
    if (b == 0xd1) {
        uint8_t v[2];
        int16_t res;

        if (read_bytes(r, v, 2)) {
            return (CFGPACK_ERR_DECODE);
        }
        res = (int16_t)(((uint16_t)v[0] << 8) | v[1]);
        *out = res;
        return (CFGPACK_OK);
    }
    if (b == 0xd2) {
        uint8_t v[4];
        int32_t res;

        if (read_bytes(r, v, 4)) {
            return (CFGPACK_ERR_DECODE);
        }
        res = (int32_t)((uint32_t)v[0] << 24 | (uint32_t)v[1] << 16 |
                        (uint32_t)v[2] << 8 | v[3]);
        *out = res;
        return (CFGPACK_OK);
    }
    if (b == 0xd3) {
        uint64_t res = 0;
        uint8_t v[8];

        if (read_bytes(r, v, 8)) {
            return (CFGPACK_ERR_DECODE);
        }
        for (int i = 0; i < 8; ++i) {
            res = (res << 8) | v[i];
        }
        *out = (int64_t)res;
        return (CFGPACK_OK);
    }
    /* cfgpack_msgpack_encode_int64 emits non-negative values in the uint
     * formats, so accept them here as long as the value fits int64_t. */
    if (decode_uint_payload(r, b, &u) != CFGPACK_OK) {
        return (CFGPACK_ERR_DECODE);
    }
    if (u > (uint64_t)INT64_MAX) {
        return (CFGPACK_ERR_DECODE);
    }
    *out = (int64_t)u;
    return (CFGPACK_OK);
}

cfgpack_err_t cfgpack_msgpack_decode_f32(cfgpack_reader_t *r, float *out) {
    uint8_t v[4];
    uint32_t u;
    uint8_t b;

    if (read_bytes(r, &b, 1)) {
        return (CFGPACK_ERR_DECODE);
    }
    if (b != 0xca) {
        return (CFGPACK_ERR_DECODE);
    }
    if (read_bytes(r, v, 4)) {
        return (CFGPACK_ERR_DECODE);
    }
    u = ((uint32_t)v[0] << 24) | ((uint32_t)v[1] << 16) |
        ((uint32_t)v[2] << 8) | v[3];
    memcpy(out, &u, sizeof(u));
    return (CFGPACK_OK);
}

cfgpack_err_t cfgpack_msgpack_decode_f64(cfgpack_reader_t *r, double *out) {
    uint64_t u = 0;
    uint8_t v[8];
    uint8_t b;

    if (read_bytes(r, &b, 1)) {
        return (CFGPACK_ERR_DECODE);
    }
    if (b != 0xcb) {
        return (CFGPACK_ERR_DECODE);
    }
    if (read_bytes(r, v, 8)) {
        return (CFGPACK_ERR_DECODE);
    }
    for (int i = 0; i < 8; ++i) {
        u = (u << 8) | v[i];
    }
    memcpy(out, &u, sizeof(u));
    return (CFGPACK_OK);
}

cfgpack_err_t cfgpack_msgpack_decode_str(cfgpack_reader_t *r,
                                         const uint8_t **ptr,
                                         uint32_t *len) {
    uint8_t b;
    if (read_bytes(r, &b, 1)) {
        return (CFGPACK_ERR_DECODE);
    }
    if ((b & 0xe0u) == 0xa0u) {
        *len = (uint32_t)(b & 0x1f);
    } else if (b == 0xd9) {
        uint8_t l;
        if (read_bytes(r, &l, 1)) {
            return (CFGPACK_ERR_DECODE);
        }
        *len = l;
    } else if (b == 0xda) {
        uint8_t v[2];
        if (read_bytes(r, v, 2)) {
            return (CFGPACK_ERR_DECODE);
        }
        *len = ((uint32_t)v[0] << 8) | v[1];
    } else {
        return (CFGPACK_ERR_DECODE);
    }
    if (*len > r->len - r->pos) {
        return (CFGPACK_ERR_DECODE);
    }
    *ptr = r->data + r->pos;
    r->pos += *len;
    return (CFGPACK_OK);
}

/**
 * @brief Advance past @p len payload bytes, refusing to run off the end.
 *
 * @p len may be attacker-controlled up to UINT32_MAX; comparing against the
 * remaining byte count cannot wrap on a 32-bit size_t.
 */
static cfgpack_err_t skip_payload(cfgpack_reader_t *r, uint32_t len) {
    if (len > r->len - r->pos) {
        return (CFGPACK_ERR_DECODE);
    }
    r->pos += len;
    return (CFGPACK_OK);
}

/**
 * @brief Read an @p n byte big-endian unsigned field (n <= 4).
 * @return CFGPACK_OK, or CFGPACK_ERR_DECODE if the input is truncated.
 */
static cfgpack_err_t read_be32(cfgpack_reader_t *r, size_t n, uint32_t *out) {
    uint32_t res = 0;
    uint8_t v[4];

    if (read_bytes(r, v, n)) {
        return (CFGPACK_ERR_DECODE);
    }
    for (size_t i = 0; i < n; ++i) {
        res = (res << 8) | v[i];
    }
    *out = res;
    return (CFGPACK_OK);
}

/**
 * @brief Consume one value's format byte and any inline payload.
 *
 * Scalars, str and bin values are consumed entirely and @p child_count is
 * set to 0.  For a non-empty map or array only the header is consumed and
 * @p child_count receives the number of child values still to skip (map
 * entries count twice).  Ext types are not supported.
 */
static cfgpack_err_t skip_header(cfgpack_reader_t *r, uint32_t *child_count) {
    cfgpack_err_t rc;
    uint32_t len;
    uint8_t b;

    *child_count = 0;
    if (r->pos >= r->len) {
        return (CFGPACK_ERR_DECODE);
    }
    b = r->data[r->pos++];

    if (b <= 0x7f || b >= 0xe0) {
        return (CFGPACK_OK); /* positive / negative fixint */
    }
    if ((b & 0xe0) == 0xa0) {
        return (skip_payload(r, b & 0x1f)); /* fixstr */
    }
    if ((b & 0xf0) == 0x80) {
        *child_count = (uint32_t)(b & 0x0f) * 2; /* fixmap */
        return (CFGPACK_OK);
    }
    if ((b & 0xf0) == 0x90) {
        *child_count = b & 0x0f; /* fixarray */
        return (CFGPACK_OK);
    }

    switch (b) {
    case 0xc0: /* nil */
    case 0xc2: /* false */
    case 0xc3: /* true */ return (CFGPACK_OK);

    case 0xcc: /* uint 8 */
    case 0xd0: /* int 8 */ return (skip_payload(r, 1));
    case 0xcd: /* uint 16 */
    case 0xd1: /* int 16 */ return (skip_payload(r, 2));
    case 0xca: /* float 32 */
    case 0xce: /* uint 32 */
    case 0xd2: /* int 32 */ return (skip_payload(r, 4));
    case 0xcb: /* float 64 */
    case 0xcf: /* uint 64 */
    case 0xd3: /* int 64 */ return (skip_payload(r, 8));

    case 0xc4: /* bin 8 */
    case 0xd9: /* str 8 */
        rc = read_be32(r, 1, &len);
        return (rc != CFGPACK_OK ? rc : skip_payload(r, len));
    case 0xc5: /* bin 16 */
    case 0xda: /* str 16 */
        rc = read_be32(r, 2, &len);
        return (rc != CFGPACK_OK ? rc : skip_payload(r, len));
    case 0xc6: /* bin 32 */
    case 0xdb: /* str 32 */
        rc = read_be32(r, 4, &len);
        return (rc != CFGPACK_OK ? rc : skip_payload(r, len));

    case 0xdc: /* array 16 */ rc = read_be32(r, 2, child_count); return (rc);
    case 0xdd: /* array 32 */ rc = read_be32(r, 4, child_count); return (rc);
    case 0xde: /* map 16 */
        rc = read_be32(r, 2, &len);
        *child_count = len * 2;
        return (rc);
    case 0xdf: /* map 32 */
        rc = read_be32(r, 4, &len);
        if (rc != CFGPACK_OK || len > UINT32_MAX / 2) {
            return (CFGPACK_ERR_DECODE);
        }
        *child_count = len * 2;
        return (CFGPACK_OK);

    default: return (CFGPACK_ERR_DECODE);
    }
}

cfgpack_err_t cfgpack_msgpack_skip_value(cfgpack_reader_t *r) {
    /*
     * Iterative msgpack value skipper with bounded stack usage.
     *
     * Instead of recursing for each nested container, we maintain an explicit
     * stack of "values remaining to skip" counters.  When a container (map or
     * array) is encountered, the number of child values is pushed; when a
     * scalar is consumed, the top counter is decremented until the container
     * is fully skipped.
     *
     * Stack budget: CFGPACK_SKIP_MAX_DEPTH * sizeof(uint32_t) bytes.
     * Default 32 levels = 128 bytes, which is safe for embedded targets.
     */
    uint32_t remaining[CFGPACK_SKIP_MAX_DEPTH];
    uint32_t children;
    uint32_t depth = 0;
    cfgpack_err_t rc;

    remaining[0] = 1; /* skip exactly one top-level value */

    do {
        rc = skip_header(r, &children);
        if (rc != CFGPACK_OK) {
            return (rc);
        }
        if (children > 0) {
            if (depth + 1 >= CFGPACK_SKIP_MAX_DEPTH) {
                return (CFGPACK_ERR_DECODE);
            }
            remaining[++depth] = children;
            continue;
        }

        /* One value finished: decrement the current container's count and
         * unwind every container that just became empty. */
        while (remaining[depth] > 0) {
            remaining[depth]--;
            if (remaining[depth] > 0) {
                break; /* more values to skip at this level */
            }
            if (depth == 0) {
                return (CFGPACK_OK); /* all done */
            }
            depth--; /* pop to parent container */
        }
    } while (depth > 0 || remaining[0] > 0);

    return (CFGPACK_OK);
}
