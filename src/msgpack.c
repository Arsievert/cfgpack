#include "cfgpack/msgpack.h"

#include "cfgpack/config.h"

#include <string.h>

void cfgpack_buf_init(cfgpack_buf_t *buf, uint8_t *storage, size_t cap) {
    buf->data = storage;
    buf->len = 0;
    buf->cap = cap;
}

cfgpack_err_t cfgpack_buf_append(cfgpack_buf_t *buf,
                                 const void *src,
                                 size_t len) {
    if (buf->len + len > buf->cap) {
        return (CFGPACK_ERR_ENCODE);
    }
    memcpy(buf->data + buf->len, src, len);
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
    return cfgpack_buf_append(buf, tmp, n);
}

cfgpack_err_t cfgpack_msgpack_encode_int64(cfgpack_buf_t *buf, int64_t v) {
    if (v >= 0) {
        return cfgpack_msgpack_encode_uint64(buf, (uint64_t)v);
    }
    uint8_t tmp[9];
    size_t n = 0;
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
    return cfgpack_buf_append(buf, tmp, n);
}

cfgpack_err_t cfgpack_msgpack_encode_f32(cfgpack_buf_t *buf, float v) {
    uint32_t u;
    memcpy(&u, &v, sizeof(u));
    uint8_t tmp[5] = {0xca, (uint8_t)(u >> 24), (uint8_t)(u >> 16),
                      (uint8_t)(u >> 8), (uint8_t)u};
    return cfgpack_buf_append(buf, tmp, sizeof(tmp));
}

cfgpack_err_t cfgpack_msgpack_encode_f64(cfgpack_buf_t *buf, double v) {
    uint64_t u;
    memcpy(&u, &v, sizeof(u));
    uint8_t tmp[9];
    tmp[0] = 0xcb;
    for (int i = 0; i < 8; ++i) {
        tmp[1 + i] = (uint8_t)(u >> (56 - 8 * i));
    }
    return cfgpack_buf_append(buf, tmp, sizeof(tmp));
}

cfgpack_err_t cfgpack_msgpack_encode_str(cfgpack_buf_t *buf,
                                         const char *s,
                                         size_t len) {
    uint8_t hdr[3];
    size_t hlen = 0;
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
    if (cfgpack_buf_append(buf, hdr, hlen) != CFGPACK_OK) {
        return CFGPACK_ERR_ENCODE;
    }
    return cfgpack_buf_append(buf, s, len);
}

cfgpack_err_t cfgpack_msgpack_encode_map_header(cfgpack_buf_t *buf,
                                                uint32_t count) {
    uint8_t tmp[3];
    size_t n = 0;
    if (count <= 15) {
        tmp[n++] = (uint8_t)(0x80 | count);
    } else {
        tmp[n++] = 0xde;
        tmp[n++] = (uint8_t)(count >> 8);
        tmp[n++] = (uint8_t)count;
    }
    return cfgpack_buf_append(buf, tmp, n);
}

cfgpack_err_t cfgpack_msgpack_encode_uint_key(cfgpack_buf_t *buf, uint64_t v) {
    return cfgpack_msgpack_encode_uint64(buf, v);
}

cfgpack_err_t cfgpack_msgpack_encode_str_key(cfgpack_buf_t *buf,
                                             const char *s,
                                             size_t len) {
    return cfgpack_msgpack_encode_str(buf, s, len);
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
        return -1;
    }
    memcpy(dst, r->data + r->pos, n);
    r->pos += n;
    return 0;
}

cfgpack_err_t cfgpack_msgpack_decode_map_header(cfgpack_reader_t *r,
                                                uint32_t *count) {
    uint8_t b;
    if (read_bytes(r, &b, 1)) {
        return CFGPACK_ERR_DECODE;
    }
    if ((b & 0xf0u) == 0x80u) {
        *count = (uint32_t)(b & 0x0f);
        return CFGPACK_OK;
    }
    if (b == 0xde) {
        uint8_t bytes[2];
        if (read_bytes(r, bytes, 2)) {
            return CFGPACK_ERR_DECODE;
        }
        *count = ((uint32_t)bytes[0] << 8) | bytes[1];
        return CFGPACK_OK;
    }
    return CFGPACK_ERR_DECODE;
}

cfgpack_err_t cfgpack_msgpack_decode_uint64(cfgpack_reader_t *r,
                                            uint64_t *out) {
    uint8_t b;
    if (read_bytes(r, &b, 1)) {
        return CFGPACK_ERR_DECODE;
    }
    if (b <= 0x7f) {
        *out = b;
        return CFGPACK_OK;
    }
    if (b == 0xcc) {
        uint8_t v;
        if (read_bytes(r, &v, 1)) {
            return CFGPACK_ERR_DECODE;
        }
        *out = v;
        return CFGPACK_OK;
    }
    if (b == 0xcd) {
        uint8_t v[2];
        if (read_bytes(r, v, 2)) {
            return CFGPACK_ERR_DECODE;
        }
        *out = ((uint16_t)v[0] << 8) | v[1];
        return CFGPACK_OK;
    }
    if (b == 0xce) {
        uint8_t v[4];
        if (read_bytes(r, v, 4)) {
            return CFGPACK_ERR_DECODE;
        }
        *out = ((uint32_t)v[0] << 24) | ((uint32_t)v[1] << 16) |
               ((uint32_t)v[2] << 8) | v[3];
        return CFGPACK_OK;
    }
    if (b == 0xcf) {
        uint8_t v[8];
        if (read_bytes(r, v, 8)) {
            return CFGPACK_ERR_DECODE;
        }
        uint64_t res = 0;
        for (int i = 0; i < 8; ++i) {
            res = (res << 8) | v[i];
        }
        *out = res;
        return CFGPACK_OK;
    }
    return CFGPACK_ERR_DECODE;
}

cfgpack_err_t cfgpack_msgpack_decode_int64(cfgpack_reader_t *r, int64_t *out) {
    uint8_t b;
    if (read_bytes(r, &b, 1)) {
        return CFGPACK_ERR_DECODE;
    }
    if (b <= 0x7f) {
        *out = (int64_t)b;
        return CFGPACK_OK;
    }
    if (b >= 0xe0) {
        *out = (int8_t)b;
        return CFGPACK_OK;
    }
    if (b == 0xd0) {
        int8_t v;
        if (read_bytes(r, &v, 1)) {
            return CFGPACK_ERR_DECODE;
        }
        *out = v;
        return CFGPACK_OK;
    }
    if (b == 0xd1) {
        int16_t v;
        if (read_bytes(r, &v, 2)) {
            return CFGPACK_ERR_DECODE;
        }
        v = (int16_t)((uint16_t)(uint8_t)(v >> 8) |
                      ((uint16_t)(uint8_t)v << 8));
        *out = v;
        return CFGPACK_OK;
    }
    if (b == 0xd2) {
        uint8_t v[4];
        if (read_bytes(r, v, 4)) {
            return CFGPACK_ERR_DECODE;
        }
        int32_t res = (int32_t)((uint32_t)v[0] << 24 | (uint32_t)v[1] << 16 |
                                (uint32_t)v[2] << 8 | v[3]);
        *out = res;
        return CFGPACK_OK;
    }
    if (b == 0xd3) {
        uint8_t v[8];
        if (read_bytes(r, v, 8)) {
            return CFGPACK_ERR_DECODE;
        }
        int64_t res = 0;
        for (int i = 0; i < 8; ++i) {
            res = (res << 8) | v[i];
        }
        *out = res;
        return CFGPACK_OK;
    }
    return CFGPACK_ERR_DECODE;
}

cfgpack_err_t cfgpack_msgpack_decode_f32(cfgpack_reader_t *r, float *out) {
    uint8_t b;
    uint8_t v[4];
    if (read_bytes(r, &b, 1)) {
        return CFGPACK_ERR_DECODE;
    }
    if (b != 0xca) {
        return CFGPACK_ERR_DECODE;
    }
    if (read_bytes(r, v, 4)) {
        return CFGPACK_ERR_DECODE;
    }
    uint32_t u = ((uint32_t)v[0] << 24) | ((uint32_t)v[1] << 16) |
                 ((uint32_t)v[2] << 8) | v[3];
    memcpy(out, &u, sizeof(u));
    return CFGPACK_OK;
}

cfgpack_err_t cfgpack_msgpack_decode_f64(cfgpack_reader_t *r, double *out) {
    uint8_t b;
    uint8_t v[8];
    if (read_bytes(r, &b, 1)) {
        return CFGPACK_ERR_DECODE;
    }
    if (b != 0xcb) {
        return CFGPACK_ERR_DECODE;
    }
    if (read_bytes(r, v, 8)) {
        return CFGPACK_ERR_DECODE;
    }
    uint64_t u = 0;
    for (int i = 0; i < 8; ++i) {
        u = (u << 8) | v[i];
    }
    memcpy(out, &u, sizeof(u));
    return CFGPACK_OK;
}

cfgpack_err_t cfgpack_msgpack_decode_str(cfgpack_reader_t *r,
                                         const uint8_t **ptr,
                                         uint32_t *len) {
    uint8_t b;
    if (read_bytes(r, &b, 1)) {
        return CFGPACK_ERR_DECODE;
    }
    if ((b & 0xe0u) == 0xa0u) {
        *len = (uint32_t)(b & 0x1f);
    } else if (b == 0xd9) {
        uint8_t l;
        if (read_bytes(r, &l, 1)) {
            return CFGPACK_ERR_DECODE;
        }
        *len = l;
    } else if (b == 0xda) {
        uint8_t v[2];
        if (read_bytes(r, v, 2)) {
            return CFGPACK_ERR_DECODE;
        }
        *len = ((uint32_t)v[0] << 8) | v[1];
    } else {
        return CFGPACK_ERR_DECODE;
    }
    if (r->pos + *len > r->len) {
        return CFGPACK_ERR_DECODE;
    }
    *ptr = r->data + r->pos;
    r->pos += *len;
    return CFGPACK_OK;
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
    uint32_t depth = 0;
    uint32_t remaining[CFGPACK_SKIP_MAX_DEPTH];
    remaining[0] = 1; /* skip exactly one top-level value */

    do {
        if (r->pos >= r->len) {
            return CFGPACK_ERR_DECODE;
        }
        uint8_t b = r->data[r->pos++];

        /* Positive fixint (0x00-0x7f) and negative fixint (0xe0-0xff) */
        if (b <= 0x7f || b >= 0xe0) {
            goto value_done;
        }

        /* Fixstr (0xa0-0xbf): length in low 5 bits */
        if ((b & 0xe0) == 0xa0) {
            uint32_t len = b & 0x1f;
            if (r->pos + len > r->len) {
                return CFGPACK_ERR_DECODE;
            }
            r->pos += len;
            goto value_done;
        }

        /* Fixmap (0x80-0x8f): count in low 4 bits, each entry = 2 values */
        if ((b & 0xf0) == 0x80) {
            uint32_t count = (uint32_t)(b & 0x0f) * 2;
            if (count == 0) {
                goto value_done;
            }
            if (depth + 1 >= CFGPACK_SKIP_MAX_DEPTH) {
                return CFGPACK_ERR_DECODE;
            }
            remaining[depth + 1] = count;
            depth++;
            continue;
        }

        /* Fixarray (0x90-0x9f): count in low 4 bits */
        if ((b & 0xf0) == 0x90) {
            uint32_t count = b & 0x0f;
            if (count == 0) {
                goto value_done;
            }
            if (depth + 1 >= CFGPACK_SKIP_MAX_DEPTH) {
                return CFGPACK_ERR_DECODE;
            }
            remaining[depth + 1] = count;
            depth++;
            continue;
        }

        /* Handle other types by format byte */
        switch (b) {
        /* nil, false, true */
        case 0xc0:
        case 0xc2:
        case 0xc3: goto value_done;

        /* bin 8, str 8 */
        case 0xc4:
        case 0xd9: {
            if (r->pos >= r->len) {
                return CFGPACK_ERR_DECODE;
            }
            uint8_t len = r->data[r->pos++];
            if (r->pos + len > r->len) {
                return CFGPACK_ERR_DECODE;
            }
            r->pos += len;
            goto value_done;
        }

        /* bin 16, str 16 */
        case 0xc5:
        case 0xda: {
            if (r->pos + 2 > r->len) {
                return CFGPACK_ERR_DECODE;
            }
            uint32_t len =
                ((uint32_t)r->data[r->pos] << 8) | r->data[r->pos + 1];
            r->pos += 2;
            if (r->pos + len > r->len) {
                return CFGPACK_ERR_DECODE;
            }
            r->pos += len;
            goto value_done;
        }

        /* bin 32, str 32 */
        case 0xc6:
        case 0xdb: {
            if (r->pos + 4 > r->len) {
                return CFGPACK_ERR_DECODE;
            }
            uint32_t len = ((uint32_t)r->data[r->pos] << 24) |
                           ((uint32_t)r->data[r->pos + 1] << 16) |
                           ((uint32_t)r->data[r->pos + 2] << 8) |
                           r->data[r->pos + 3];
            r->pos += 4;
            if (r->pos + len > r->len) {
                return CFGPACK_ERR_DECODE;
            }
            r->pos += len;
            goto value_done;
        }

        /* float 32, uint 32, int 32 */
        case 0xca:
        case 0xce:
        case 0xd2:
            if (r->pos + 4 > r->len) {
                return CFGPACK_ERR_DECODE;
            }
            r->pos += 4;
            goto value_done;

        /* float 64, uint 64, int 64 */
        case 0xcb:
        case 0xcf:
        case 0xd3:
            if (r->pos + 8 > r->len) {
                return CFGPACK_ERR_DECODE;
            }
            r->pos += 8;
            goto value_done;

        /* uint 8, int 8 */
        case 0xcc:
        case 0xd0:
            if (r->pos + 1 > r->len) {
                return CFGPACK_ERR_DECODE;
            }
            r->pos += 1;
            goto value_done;

        /* uint 16, int 16 */
        case 0xcd:
        case 0xd1:
            if (r->pos + 2 > r->len) {
                return CFGPACK_ERR_DECODE;
            }
            r->pos += 2;
            goto value_done;

        /* array 16 */
        case 0xdc: {
            if (r->pos + 2 > r->len) {
                return CFGPACK_ERR_DECODE;
            }
            uint32_t count =
                ((uint32_t)r->data[r->pos] << 8) | r->data[r->pos + 1];
            r->pos += 2;
            if (count == 0) {
                goto value_done;
            }
            if (depth + 1 >= CFGPACK_SKIP_MAX_DEPTH) {
                return CFGPACK_ERR_DECODE;
            }
            remaining[depth + 1] = count;
            depth++;
            continue;
        }

        /* array 32 */
        case 0xdd: {
            if (r->pos + 4 > r->len) {
                return CFGPACK_ERR_DECODE;
            }
            uint32_t count = ((uint32_t)r->data[r->pos] << 24) |
                             ((uint32_t)r->data[r->pos + 1] << 16) |
                             ((uint32_t)r->data[r->pos + 2] << 8) |
                             r->data[r->pos + 3];
            r->pos += 4;
            if (count == 0) {
                goto value_done;
            }
            if (depth + 1 >= CFGPACK_SKIP_MAX_DEPTH) {
                return CFGPACK_ERR_DECODE;
            }
            remaining[depth + 1] = count;
            depth++;
            continue;
        }

        /* map 16 */
        case 0xde: {
            if (r->pos + 2 > r->len) {
                return CFGPACK_ERR_DECODE;
            }
            uint32_t count =
                ((uint32_t)r->data[r->pos] << 8) | r->data[r->pos + 1];
            r->pos += 2;
            if (count == 0) {
                goto value_done;
            }
            if (depth + 1 >= CFGPACK_SKIP_MAX_DEPTH) {
                return CFGPACK_ERR_DECODE;
            }
            remaining[depth + 1] = count * 2;
            depth++;
            continue;
        }

        /* map 32 */
        case 0xdf: {
            if (r->pos + 4 > r->len) {
                return CFGPACK_ERR_DECODE;
            }
            uint32_t count = ((uint32_t)r->data[r->pos] << 24) |
                             ((uint32_t)r->data[r->pos + 1] << 16) |
                             ((uint32_t)r->data[r->pos + 2] << 8) |
                             r->data[r->pos + 3];
            r->pos += 4;
            if (count == 0) {
                goto value_done;
            }
            if (depth + 1 >= CFGPACK_SKIP_MAX_DEPTH) {
                return CFGPACK_ERR_DECODE;
            }
            remaining[depth + 1] = count * 2;
            depth++;
            continue;
        }

        default: return CFGPACK_ERR_DECODE;
        }

    value_done:
        /* Decrement the current container's remaining count and unwind. */
        while (remaining[depth] > 0) {
            remaining[depth]--;
            if (remaining[depth] > 0) {
                break; /* more values to skip at this level */
            }
            if (depth == 0) {
                return CFGPACK_OK; /* all done */
            }
            depth--; /* pop to parent container */
        }
    } while (depth > 0 || remaining[0] > 0);

    return CFGPACK_OK;
}
