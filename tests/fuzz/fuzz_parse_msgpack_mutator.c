/**
 * @file fuzz_parse_msgpack_mutator.c
 * @brief Structure-aware libFuzzer harness for the MessagePack schema parser.
 *
 * Uses LLVMFuzzerCustomMutator to generate inputs that maintain valid
 * msgpack schema structure while mutating field values, entry counts,
 * types, and defaults.  This complements fuzz_parse_msgpack.c (which
 * feeds pure random bytes) by reaching deeper parser code paths that
 * require structurally valid msgpack containers.
 *
 * The mutator interprets the fuzzer-provided random data as parameters
 * that drive schema generation, so every input produces a well-formed
 * msgpack blob with targeted corruption applied on top.
 */

#include "cfgpack/cfgpack.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* ── msgpack encoding helpers (self-contained, no library dependency) ──── */

typedef struct {
    uint8_t *data;
    size_t cap;
    size_t len;
} wbuf_t;

static void wbuf_init(wbuf_t *b, uint8_t *storage, size_t cap) {
    b->data = storage;
    b->cap = cap;
    b->len = 0;
}

static int wbuf_append(wbuf_t *b, const void *src, size_t n) {
    if (b->len + n > b->cap) {
        return -1;
    }
    memcpy(b->data + b->len, src, n);
    b->len += n;
    return 0;
}

static int wbuf_u8(wbuf_t *b, uint8_t v) {
    return wbuf_append(b, &v, 1);
}

static int mp_nil(wbuf_t *b) {
    return wbuf_u8(b, 0xC0);
}

static int mp_uint(wbuf_t *b, uint64_t v) {
    if (v <= 0x7F) {
        return wbuf_u8(b, (uint8_t)v);
    } else if (v <= 0xFF) {
        uint8_t buf[2] = {0xCC, (uint8_t)v};
        return wbuf_append(b, buf, 2);
    } else if (v <= 0xFFFF) {
        uint8_t buf[3] = {0xCD, (uint8_t)(v >> 8), (uint8_t)v};
        return wbuf_append(b, buf, 3);
    } else if (v <= 0xFFFFFFFF) {
        uint8_t buf[5] = {0xCE, (uint8_t)(v >> 24), (uint8_t)(v >> 16),
                          (uint8_t)(v >> 8), (uint8_t)v};
        return wbuf_append(b, buf, 5);
    } else {
        uint8_t buf[9] = {0xCF,
                          (uint8_t)(v >> 56),
                          (uint8_t)(v >> 48),
                          (uint8_t)(v >> 40),
                          (uint8_t)(v >> 32),
                          (uint8_t)(v >> 24),
                          (uint8_t)(v >> 16),
                          (uint8_t)(v >> 8),
                          (uint8_t)v};
        return wbuf_append(b, buf, 9);
    }
}

static int mp_int(wbuf_t *b, int64_t v) {
    if (v >= 0) {
        return mp_uint(b, (uint64_t)v);
    }
    if (v >= -32) {
        return wbuf_u8(b, (uint8_t)v); /* negative fixint */
    } else if (v >= -128) {
        uint8_t buf[2] = {0xD0, (uint8_t)v};
        return wbuf_append(b, buf, 2);
    } else if (v >= -32768) {
        uint8_t buf[3] = {0xD1, (uint8_t)((uint16_t)v >> 8), (uint8_t)v};
        return wbuf_append(b, buf, 3);
    } else if (v >= INT32_MIN) {
        uint32_t u = (uint32_t)v;
        uint8_t buf[5] = {0xD2, (uint8_t)(u >> 24), (uint8_t)(u >> 16),
                          (uint8_t)(u >> 8), (uint8_t)u};
        return wbuf_append(b, buf, 5);
    } else {
        uint64_t u = (uint64_t)v;
        uint8_t buf[9] = {0xD3,
                          (uint8_t)(u >> 56),
                          (uint8_t)(u >> 48),
                          (uint8_t)(u >> 40),
                          (uint8_t)(u >> 32),
                          (uint8_t)(u >> 24),
                          (uint8_t)(u >> 16),
                          (uint8_t)(u >> 8),
                          (uint8_t)u};
        return wbuf_append(b, buf, 9);
    }
}

static int mp_f32(wbuf_t *b, float v) {
    uint32_t u;
    memcpy(&u, &v, 4);
    uint8_t buf[5] = {0xCA, (uint8_t)(u >> 24), (uint8_t)(u >> 16),
                      (uint8_t)(u >> 8), (uint8_t)u};
    return wbuf_append(b, buf, 5);
}

static int mp_f64(wbuf_t *b, double v) {
    uint64_t u;
    memcpy(&u, &v, 8);
    uint8_t buf[9] = {0xCB,
                      (uint8_t)(u >> 56),
                      (uint8_t)(u >> 48),
                      (uint8_t)(u >> 40),
                      (uint8_t)(u >> 32),
                      (uint8_t)(u >> 24),
                      (uint8_t)(u >> 16),
                      (uint8_t)(u >> 8),
                      (uint8_t)u};
    return wbuf_append(b, buf, 9);
}

static int mp_str(wbuf_t *b, const char *s, size_t len) {
    if (len <= 31) {
        if (wbuf_u8(b, (uint8_t)(0xA0 | len))) {
            return -1;
        }
    } else if (len <= 255) {
        uint8_t hdr[2] = {0xD9, (uint8_t)len};
        if (wbuf_append(b, hdr, 2)) {
            return -1;
        }
    } else if (len <= 65535) {
        uint8_t hdr[3] = {0xDA, (uint8_t)(len >> 8), (uint8_t)len};
        if (wbuf_append(b, hdr, 3)) {
            return -1;
        }
    } else {
        return -1;
    }
    return wbuf_append(b, s, len);
}

static int mp_fixmap(wbuf_t *b, uint8_t count) {
    return wbuf_u8(b, (uint8_t)(0x80 | (count & 0x0F)));
}

static int mp_map16(wbuf_t *b, uint16_t count) {
    uint8_t hdr[3] = {0xDE, (uint8_t)(count >> 8), (uint8_t)count};
    return wbuf_append(b, hdr, 3);
}

static int mp_fixarray(wbuf_t *b, uint8_t count) {
    return wbuf_u8(b, (uint8_t)(0x90 | (count & 0x0F)));
}

static int mp_array16(wbuf_t *b, uint16_t count) {
    uint8_t hdr[3] = {0xDC, (uint8_t)(count >> 8), (uint8_t)count};
    return wbuf_append(b, hdr, 3);
}

/* ── random byte consumer ─────────────────────────────────────────────── */

typedef struct {
    const uint8_t *data;
    size_t len;
    size_t pos;
} rng_t;

static uint8_t rng_u8(rng_t *r) {
    if (r->pos >= r->len) {
        return 0;
    }
    return r->data[r->pos++];
}

static uint16_t rng_u16(rng_t *r) {
    uint16_t v = rng_u8(r);
    v = (uint16_t)((v << 8) | rng_u8(r));
    return v;
}

static uint32_t rng_u32(rng_t *r) {
    uint32_t v = rng_u16(r);
    v = (v << 16) | rng_u16(r);
    return v;
}

static uint64_t rng_u64(rng_t *r) {
    uint64_t v = rng_u32(r);
    v = (v << 32) | rng_u32(r);
    return v;
}

/* ── corruption modes ─────────────────────────────────────────────────── */

enum {
    /* No corruption — fully valid schema */
    CORRUPT_NONE = 0,
    /* Emit wrong map count at top level */
    CORRUPT_TOP_MAP_COUNT,
    /* Emit wrong array count for entries */
    CORRUPT_ARRAY_COUNT,
    /* Emit wrong map count for an entry */
    CORRUPT_ENTRY_MAP_COUNT,
    /* Use wrong key numbers at top level */
    CORRUPT_TOP_KEYS,
    /* Use wrong key numbers in an entry */
    CORRUPT_ENTRY_KEYS,
    /* Mismatch type and default value encoding */
    CORRUPT_TYPE_VALUE_MISMATCH,
    /* Duplicate entry index */
    CORRUPT_DUPLICATE_INDEX,
    /* Duplicate entry name */
    CORRUPT_DUPLICATE_NAME,
    /* Entry index out of valid range (0 or > 65535) */
    CORRUPT_INDEX_RANGE,
    /* Entry name too long or empty */
    CORRUPT_NAME_LENGTH,
    /* Entry type out of enum range */
    CORRUPT_TYPE_RANGE,
    /* Truncate the output at a random point */
    CORRUPT_TRUNCATE,
    /* Flip random bytes in the output */
    CORRUPT_BITFLIP,
    /* Use map16 instead of fixmap at various levels */
    CORRUPT_MAP16_ENCODING,
    /* Append garbage after valid schema */
    CORRUPT_TRAILING_GARBAGE,

    CORRUPT_COUNT
};

/* ── schema generation from random data ───────────────────────────────── */

#define MAX_ENTRIES 10
#define BUF_SIZE 4096

/* Type enum values matching cfgpack_type_t */
enum {
    TYPE_U8 = 0,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_I64,
    TYPE_F32,
    TYPE_F64,
    TYPE_STR,
    TYPE_FSTR,
    TYPE_COUNT = 12
};

/**
 * Encode a default value for the given type, consuming randomness.
 * If is_nil is set, encode nil instead (no default).
 */
static int encode_default(wbuf_t *b, uint8_t type, int is_nil, rng_t *r) {
    if (is_nil) {
        return mp_nil(b);
    }

    switch (type) {
    case TYPE_U8: return mp_uint(b, rng_u8(r));
    case TYPE_U16: return mp_uint(b, rng_u16(r));
    case TYPE_U32: return mp_uint(b, rng_u32(r));
    case TYPE_U64: return mp_uint(b, rng_u64(r));
    case TYPE_I8: return mp_int(b, (int8_t)rng_u8(r));
    case TYPE_I16: return mp_int(b, (int16_t)rng_u16(r));
    case TYPE_I32: return mp_int(b, (int32_t)rng_u32(r));
    case TYPE_I64: return mp_int(b, (int64_t)rng_u64(r));
    case TYPE_F32: {
        float v;
        uint32_t bits = rng_u32(r);
        memcpy(&v, &bits, 4);
        return mp_f32(b, v);
    }
    case TYPE_F64: {
        double v;
        uint64_t bits = rng_u64(r);
        memcpy(&v, &bits, 8);
        return mp_f64(b, v);
    }
    case TYPE_STR: {
        uint8_t len = (uint8_t)(rng_u8(r) & 0x3F); /* 0..63 */
        char tmp[64];
        for (uint8_t i = 0; i < len; ++i) {
            tmp[i] = (char)(0x20 + (rng_u8(r) % 95)); /* printable */
        }
        return mp_str(b, tmp, len);
    }
    case TYPE_FSTR: {
        uint8_t len = (uint8_t)(rng_u8(r) & 0x0F); /* 0..15 */
        char tmp[16];
        for (uint8_t i = 0; i < len; ++i) {
            tmp[i] = (char)(0x20 + (rng_u8(r) % 95));
        }
        return mp_str(b, tmp, len);
    }
    default: return mp_nil(b);
    }
}

/**
 * Encode a deliberately wrong default value (type different from
 * what the entry's type field claims).
 */
static int encode_mismatched_default(wbuf_t *b, uint8_t type, rng_t *r) {
    /* Pick a type that differs from the declared type */
    uint8_t alt = (uint8_t)((type + 1 + (rng_u8(r) % (TYPE_COUNT - 1))) %
                            TYPE_COUNT);
    return encode_default(b, alt, 0, r);
}

static size_t generate_schema(uint8_t *out,
                              size_t max_len,
                              const uint8_t *seed,
                              size_t seed_len) {
    rng_t r = {seed, seed_len, 0};
    wbuf_t b;
    wbuf_init(&b, out, max_len);

    /* Pick corruption mode */
    uint8_t corrupt = (uint8_t)(rng_u8(&r) % CORRUPT_COUNT);

    /* Schema name: 1..5 chars */
    uint8_t name_len = (uint8_t)(1 + (rng_u8(&r) % 5));
    char name[6];
    for (uint8_t i = 0; i < name_len; ++i) {
        name[i] = (char)('a' + (rng_u8(&r) % 26));
    }
    name[name_len] = '\0';

    /* Version: small number usually, occasionally large */
    uint32_t version = (uint32_t)(rng_u8(&r) & 0x01)
                           ? rng_u32(&r)
                           : (uint32_t)(1 + (rng_u8(&r) % 10));

    /* Entry count: 1..MAX_ENTRIES */
    uint8_t entry_count = (uint8_t)(1 + (rng_u8(&r) % MAX_ENTRIES));

    /* --- Top-level map header --- */
    uint8_t top_map_count = 3;
    if (corrupt == CORRUPT_TOP_MAP_COUNT) {
        top_map_count = rng_u8(&r) % 8; /* 0..7, likely wrong */
    }
    if (corrupt == CORRUPT_MAP16_ENCODING) {
        mp_map16(&b, top_map_count);
    } else {
        mp_fixmap(&b, top_map_count);
    }

    /* --- Key 0: name --- */
    uint8_t k0 = 0, k1 = 1, k2 = 2;
    if (corrupt == CORRUPT_TOP_KEYS) {
        k0 = rng_u8(&r) % 8;
        k1 = rng_u8(&r) % 8;
        k2 = rng_u8(&r) % 8;
    }
    mp_uint(&b, k0);

    /* name value — sometimes corrupt the length */
    if (corrupt == CORRUPT_NAME_LENGTH && entry_count == 1) {
        /* Put the name corruption at the schema level too for variety */
        mp_str(&b, name, name_len);
    } else {
        mp_str(&b, name, name_len);
    }

    /* --- Key 1: version --- */
    mp_uint(&b, k1);
    mp_uint(&b, version);

    /* --- Key 2: entries --- */
    mp_uint(&b, k2);

    /* Array header for entries */
    uint8_t arr_count = entry_count;
    if (corrupt == CORRUPT_ARRAY_COUNT) {
        /* Claim a different number of entries than we actually encode */
        arr_count = rng_u8(&r) % 16;
    }
    if (arr_count <= 15) {
        mp_fixarray(&b, arr_count);
    } else {
        mp_array16(&b, arr_count);
    }

    /* Pre-generate entry indices and names for dedup corruption */
    uint16_t indices[MAX_ENTRIES];
    char names[MAX_ENTRIES][6];
    uint8_t name_lens[MAX_ENTRIES];

    for (uint8_t i = 0; i < entry_count; ++i) {
        indices[i] = (uint16_t)(1 + (rng_u16(&r) % 127)); /* 1..127 */
        name_lens[i] = (uint8_t)(1 + (rng_u8(&r) % 5));
        for (uint8_t j = 0; j < name_lens[i]; ++j) {
            names[i][j] = (char)('a' + (rng_u8(&r) % 26));
        }
        names[i][name_lens[i]] = '\0';
    }

    /* Apply dedup corruptions */
    if (corrupt == CORRUPT_DUPLICATE_INDEX && entry_count >= 2) {
        indices[1] = indices[0];
    }
    if (corrupt == CORRUPT_DUPLICATE_NAME && entry_count >= 2) {
        name_lens[1] = name_lens[0];
        memcpy(names[1], names[0], name_lens[0] + 1);
    }

    /* --- Encode each entry --- */
    for (uint8_t i = 0; i < entry_count; ++i) {
        /* Entry map header */
        uint8_t emap_count = 4;
        if (corrupt == CORRUPT_ENTRY_MAP_COUNT) {
            emap_count = rng_u8(&r) % 8;
        }
        if (corrupt == CORRUPT_MAP16_ENCODING && (i & 1)) {
            mp_map16(&b, emap_count);
        } else {
            mp_fixmap(&b, emap_count);
        }

        /* Entry keys */
        uint8_t ek0 = 0, ek1 = 1, ek2 = 2, ek3 = 3;
        if (corrupt == CORRUPT_ENTRY_KEYS) {
            ek0 = rng_u8(&r) % 8;
            ek1 = rng_u8(&r) % 8;
            ek2 = rng_u8(&r) % 8;
            ek3 = rng_u8(&r) % 8;
        }

        /* key 0: index */
        mp_uint(&b, ek0);
        if (corrupt == CORRUPT_INDEX_RANGE && i == 0) {
            /* Emit index 0 (reserved) or a huge value */
            if (rng_u8(&r) & 1) {
                mp_uint(&b, 0);
            } else {
                mp_uint(&b, 0x10000); /* > 65535 */
            }
        } else {
            mp_uint(&b, indices[i]);
        }

        /* key 1: name */
        mp_uint(&b, ek1);
        if (corrupt == CORRUPT_NAME_LENGTH && i == 0) {
            if (rng_u8(&r) & 1) {
                /* Empty name */
                mp_str(&b, "", 0);
            } else {
                /* 6-char name (too long) */
                mp_str(&b, "abcdef", 6);
            }
        } else {
            mp_str(&b, names[i], name_lens[i]);
        }

        /* key 2: type */
        mp_uint(&b, ek2);
        uint8_t type = (uint8_t)(rng_u8(&r) % TYPE_COUNT);
        if (corrupt == CORRUPT_TYPE_RANGE && i == 0) {
            type = (uint8_t)(TYPE_COUNT + (rng_u8(&r) % 244)); /* 12..255 */
        }
        mp_uint(&b, type);

        /* key 3: default value */
        mp_uint(&b, ek3);
        int is_nil = (rng_u8(&r) & 3) == 0; /* ~25% chance of nil */
        if (corrupt == CORRUPT_TYPE_VALUE_MISMATCH && i == 0) {
            encode_mismatched_default(&b, type, &r);
        } else {
            uint8_t val_type = (corrupt == CORRUPT_TYPE_RANGE && i == 0)
                                   ? (uint8_t)(rng_u8(&r) % TYPE_COUNT)
                                   : type;
            encode_default(&b, val_type, is_nil, &r);
        }

        if (b.len >= b.cap) {
            break; /* stop if buffer is full */
        }
    }

    size_t final_len = b.len;

    /* --- Post-generation corruption --- */
    if (corrupt == CORRUPT_TRUNCATE && final_len > 2) {
        /* Cut at a random point */
        size_t cut = 1 + (rng_u16(&r) % (uint16_t)(final_len - 1));
        if (cut < final_len) {
            final_len = cut;
        }
    }
    if (corrupt == CORRUPT_BITFLIP && final_len > 0) {
        /* Flip 1..4 random bytes */
        uint8_t flips = (uint8_t)(1 + (rng_u8(&r) % 4));
        for (uint8_t f = 0; f < flips; ++f) {
            size_t pos = rng_u16(&r) % final_len;
            out[pos] ^= (uint8_t)(1 << (rng_u8(&r) % 8));
        }
    }
    if (corrupt == CORRUPT_TRAILING_GARBAGE && final_len + 16 <= max_len) {
        uint8_t garb_len = (uint8_t)(1 + (rng_u8(&r) % 16));
        for (uint8_t g = 0; g < garb_len && final_len < max_len; ++g) {
            out[final_len++] = rng_u8(&r);
        }
    }

    return final_len;
}

/* ── libFuzzer custom mutator ─────────────────────────────────────────── */

size_t LLVMFuzzerCustomMutator(uint8_t *data,
                               size_t size,
                               size_t max_size,
                               unsigned int seed) {
    /*
     * Strategy: use the incoming data as a random seed to generate a
     * structurally valid (or precisely corrupted) msgpack schema blob.
     *
     * libFuzzer's coverage feedback still works: it will evolve the
     * seed bytes to explore different generation paths, entry counts,
     * types, corruption modes, etc.
     *
     * We also mix in the libFuzzer-provided seed for additional entropy
     * so that different mutation rounds starting from the same input
     * can produce different outputs.
     */
    uint8_t combined[8200];
    size_t combined_len = 0;

    /* Prepend the libFuzzer seed as 4 extra randomness bytes */
    combined[0] = (uint8_t)(seed >> 24);
    combined[1] = (uint8_t)(seed >> 16);
    combined[2] = (uint8_t)(seed >> 8);
    combined[3] = (uint8_t)(seed);
    combined_len = 4;

    /* Append the existing data */
    size_t copy_len = size;
    if (copy_len > sizeof(combined) - 4) {
        copy_len = sizeof(combined) - 4;
    }
    memcpy(combined + 4, data, copy_len);
    combined_len += copy_len;

    size_t out_len = generate_schema(data, max_size, combined, combined_len);
    return out_len > 0 ? out_len : 1;
}

/* ── libFuzzer test entry point ───────────────────────────────────────── */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 8192) {
        return 0;
    }

    cfgpack_parse_error_t err;

    /* --- Exercise measure (pre-parse sizing scan) --- */
    cfgpack_schema_measure_t measure;
    (void)cfgpack_schema_measure_msgpack(data, size, &measure, &err);

    /* --- Exercise full parse ---
     * Use static buffers to avoid stack overflow under ASan (which adds
     * red zones around every stack variable).  libFuzzer is single-threaded
     * so static is safe here. */
    static cfgpack_schema_t schema;
    static cfgpack_entry_t entries[16];
    static cfgpack_value_t values[16];
    static char str_pool[512];
    static uint16_t str_offsets[16];

    cfgpack_parse_opts_t opts = {
        &schema,          entries,     16, values, str_pool,
        sizeof(str_pool), str_offsets, 16, &err,
    };
    cfgpack_err_t rc = cfgpack_schema_parse_msgpack(data, size, &opts);

    /* If parse succeeded, exercise init to stress validation paths. */
    if (rc == CFGPACK_OK && schema.entry_count > 0 &&
        schema.entry_count <= 16) {
        static cfgpack_ctx_t ctx;
        rc = cfgpack_init(&ctx, &schema, values, schema.entry_count, str_pool,
                          sizeof(str_pool), str_offsets,
                          opts.str_offsets_count);
        if (rc == CFGPACK_OK) {
            /* Roundtrip: pageout then pagein to stress deeper I/O paths.
             * Use stack-allocated buffers (not static) to avoid
             * memcpy-param-overlap between adjacent static globals
             * under ASan.  2 KiB is plenty for small schemas. */
            uint8_t po_buf[2048];
            size_t po_len = 0;
            cfgpack_err_t po_rc = cfgpack_pageout(&ctx, po_buf, sizeof(po_buf),
                                                  &po_len);
            if (po_rc == CFGPACK_OK && po_len > 0) {
                memset(ctx.present, 0, sizeof(ctx.present));
                (void)cfgpack_pagein_buf(&ctx, po_buf, po_len);
            }
            cfgpack_free(&ctx);
        }
    }

    return 0;
}
