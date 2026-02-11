#include "cfgpack/schema.h"
#include "cfgpack/api.h"
#include "cfgpack/error.h"
#include "cfgpack/value.h"
#include "cfgpack/config.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "tokens.h"

#define MAX_LINE_LEN 256

/* ─────────────────────────────────────────────────────────────────────────────
 * Internal fat value type - used on the stack during parsing only
 * ───────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Fat value container with inline string storage (internal only).
 *
 * Used transiently on the stack during parsing to hold string default data
 * before it is written to the string pool. Not exposed in public headers.
 */
typedef struct {
    cfgpack_type_t type;
    union {
        uint64_t u64;
        int64_t i64;
        float f32;
        double f64;
        struct {
            uint16_t len;
            char data[CFGPACK_STR_MAX + 1];
        } str;
        struct {
            uint8_t len;
            char data[CFGPACK_FSTR_MAX + 1];
        } fstr;
    } v;
} cfgpack_fat_value_t;

/* ─────────────────────────────────────────────────────────────────────────────
 * Write Buffer Helper - writes to caller-provided buffer, tracks total needed
 * ───────────────────────────────────────────────────────────────────────────── */

typedef struct {
    char *buf;
    size_t cap;
    size_t len; /* Always tracks total needed, even if > cap */
} wbuf_t;

static void wbuf_init(wbuf_t *w, char *buf, size_t cap) {
    w->buf = buf;
    w->cap = cap;
    w->len = 0;
}

static void wbuf_append(wbuf_t *w, const char *s, size_t n) {
    if (w->len + n <= w->cap) {
        memcpy(w->buf + w->len, s, n);
    } else if (w->len < w->cap) {
        /* Partial write */
        size_t avail = w->cap - w->len;
        memcpy(w->buf + w->len, s, avail);
    }
    w->len += n; /* Always increment to track needed size */
}

static void wbuf_puts(wbuf_t *w, const char *s) {
    wbuf_append(w, s, strlen(s));
}

static void wbuf_putc(wbuf_t *w, char c) {
    wbuf_append(w, &c, 1);
}

/* Simple integer to string (no snprintf needed for small ints) */
static void wbuf_put_uint(wbuf_t *w, unsigned long long val) {
    char tmp[24];
    int i = 0;
    if (val == 0) {
        wbuf_putc(w, '0');
        return;
    }
    while (val > 0) {
        tmp[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0) {
        wbuf_putc(w, tmp[--i]);
    }
}

static void wbuf_put_int(wbuf_t *w, long long val) {
    if (val < 0) {
        wbuf_putc(w, '-');
        val = -val;
    }
    wbuf_put_uint(w, (unsigned long long)val);
}

/* Format a double - minimal implementation for embedded, snprintf for hosted */
static void wbuf_put_double(wbuf_t *w, double val) {
#ifdef CFGPACK_HOSTED
    char tmp[32];
    int len = snprintf(tmp, sizeof(tmp), "%.17g", val);
    if (len > 0 && (size_t)len < sizeof(tmp)) {
        wbuf_append(w, tmp, (size_t)len);
    }
#else
    /* Minimal formatter: sign, integer part, decimal point, up to 9 fractional digits */
    if (val < 0) {
        wbuf_putc(w, '-');
        val = -val;
    }
    uint64_t ipart = (uint64_t)val;
    wbuf_put_uint(w, ipart);

    double frac = val - (double)ipart;
    if (frac > 1e-10) { /* Skip if essentially zero */
        wbuf_putc(w, '.');
        /* Multiply by 10^9 and round to get all digits at once - avoids accumulated error */
        uint64_t frac_int = (uint64_t)(frac * 1000000000.0 + 0.5);
        /* Extract up to 9 digits */
        char digits[9];
        int ndigits = 9;
        for (int i = 8; i >= 0; i--) {
            digits[i] = '0' + (frac_int % 10);
            frac_int /= 10;
        }
        /* Trim trailing zeros */
        while (ndigits > 0 && digits[ndigits - 1] == '0') {
            ndigits--;
        }
        for (int i = 0; i < ndigits; i++) {
            wbuf_putc(w, digits[i]);
        }
    }
#endif
}

static void wbuf_put_float(wbuf_t *w, float val) {
#ifdef CFGPACK_HOSTED
    char tmp[32];
    int len = snprintf(tmp, sizeof(tmp), "%.9g", (double)val);
    if (len > 0 && (size_t)len < sizeof(tmp)) {
        wbuf_append(w, tmp, (size_t)len);
    }
#else
    wbuf_put_double(w, (double)val);
#endif
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Line Iterator - reads lines from buffer without modifying it
 * ───────────────────────────────────────────────────────────────────────────── */

typedef struct {
    const char *data;
    size_t len;
    size_t pos;
} line_iter_t;

static void line_iter_init(line_iter_t *it, const char *data, size_t len) {
    it->data = data;
    it->len = len;
    it->pos = 0;
}

/* Returns pointer to line start, sets *line_len, advances pos.
 * Returns NULL when no more lines. Line does NOT include newline. */
static const char *line_iter_next(line_iter_t *it, size_t *line_len) {
    if (it->pos >= it->len) {
        return NULL;
    }

    const char *start = it->data + it->pos;
    size_t remaining = it->len - it->pos;
    size_t i = 0;

    /* Find end of line (newline or end of buffer) */
    while (i < remaining && start[i] != '\n' && start[i] != '\r') {
        i++;
    }

    *line_len = i;

    /* Advance past the line and any newline characters */
    it->pos += i;
    if (it->pos < it->len && it->data[it->pos] == '\r') {
        it->pos++;
    }
    if (it->pos < it->len && it->data[it->pos] == '\n') {
        it->pos++;
    }

    return start;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Parser Helpers
 * ───────────────────────────────────────────────────────────────────────────── */

/**
 * @brief Populate a parse error structure.
 */
static void set_err(cfgpack_parse_error_t *err, size_t line, const char *msg) {
    if (!err) {
        return;
    }
    err->line = line;
    size_t msg_len = strlen(msg);
    if (msg_len >= sizeof(err->message)) {
        msg_len = sizeof(err->message) - 1;
    }
    memcpy(err->message, msg, msg_len);
    err->message[msg_len] = '\0';
}

/**
 * @brief Check if a character is whitespace.
 */
static int is_space(unsigned char c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r');
}

/**
 * @brief Check if a line is blank or a comment.
 */
static int is_blank_or_comment_n(const char *line, size_t len) {
    size_t i = 0;
    while (i < len && is_space((unsigned char)line[i])) {
        i++;
    }
    return (i >= len || line[i] == '#');
}

/**
 * @brief Parse a type string into a cfgpack_type_t.
 */
static cfgpack_err_t parse_type(const char *tok, cfgpack_type_t *out) {
    if (strcmp(tok, "u8") == 0) {
        *out = CFGPACK_TYPE_U8;
        return CFGPACK_OK;
    }
    if (strcmp(tok, "u16") == 0) {
        *out = CFGPACK_TYPE_U16;
        return CFGPACK_OK;
    }
    if (strcmp(tok, "u32") == 0) {
        *out = CFGPACK_TYPE_U32;
        return CFGPACK_OK;
    }
    if (strcmp(tok, "u64") == 0) {
        *out = CFGPACK_TYPE_U64;
        return CFGPACK_OK;
    }
    if (strcmp(tok, "i8") == 0) {
        *out = CFGPACK_TYPE_I8;
        return CFGPACK_OK;
    }
    if (strcmp(tok, "i16") == 0) {
        *out = CFGPACK_TYPE_I16;
        return CFGPACK_OK;
    }
    if (strcmp(tok, "i32") == 0) {
        *out = CFGPACK_TYPE_I32;
        return CFGPACK_OK;
    }
    if (strcmp(tok, "i64") == 0) {
        *out = CFGPACK_TYPE_I64;
        return CFGPACK_OK;
    }
    if (strcmp(tok, "f32") == 0) {
        *out = CFGPACK_TYPE_F32;
        return CFGPACK_OK;
    }
    if (strcmp(tok, "f64") == 0) {
        *out = CFGPACK_TYPE_F64;
        return CFGPACK_OK;
    }
    if (strcmp(tok, "str") == 0) {
        *out = CFGPACK_TYPE_STR;
        return CFGPACK_OK;
    }
    if (strcmp(tok, "fstr") == 0) {
        *out = CFGPACK_TYPE_FSTR;
        return CFGPACK_OK;
    }
    return CFGPACK_ERR_INVALID_TYPE;
}

/**
 * @brief Check if an entry name exceeds the allowed length.
 */
static int name_too_long(const char *name) {
    return strlen(name) > 5 || strlen(name) == 0;
}

/**
 * @brief Check for duplicate index or name in existing entries.
 */
static int has_duplicate(const cfgpack_entry_t *entries, size_t count, uint16_t idx, const char *name) {
    for (size_t i = 0; i < count; ++i) {
        if (entries[i].index == idx) {
            return 1;
        }
        if (strcmp(entries[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief Sort entries and values by index using insertion sort.
 */
static void sort_entries(cfgpack_entry_t *entries, cfgpack_value_t *values, size_t n) {
    for (size_t i = 1; i < n; ++i) {
        cfgpack_entry_t key_entry = entries[i];
        cfgpack_value_t key_value = values[i];
        size_t j = i;
        while (j > 0 && entries[j - 1].index > key_entry.index) {
            entries[j] = entries[j - 1];
            values[j] = values[j - 1];
            --j;
        }
        entries[j] = key_entry;
        values[j] = key_value;
    }
}

/**
 * @brief Compute string pool offsets for sorted entries.
 *
 * After sorting, recomputes str_offsets[] in sorted entry order.
 * Returns total pool bytes needed.
 */
static size_t compute_str_offsets(const cfgpack_entry_t *entries,
                                  size_t count,
                                  uint16_t *str_offsets,
                                  size_t str_offsets_count) {
    size_t slot = 0;
    size_t pool_offset = 0;
    for (size_t i = 0; i < count; ++i) {
        cfgpack_type_t t = entries[i].type;
        if (t == CFGPACK_TYPE_STR) {
            if (slot < str_offsets_count) {
                str_offsets[slot] = (uint16_t)pool_offset;
            }
            slot++;
            pool_offset += CFGPACK_STR_MAX + 1;
        } else if (t == CFGPACK_TYPE_FSTR) {
            if (slot < str_offsets_count) {
                str_offsets[slot] = (uint16_t)pool_offset;
            }
            slot++;
            pool_offset += CFGPACK_FSTR_MAX + 1;
        }
    }
    return pool_offset;
}

/**
 * @brief Get the string slot index for a given entry offset.
 */
static int get_str_slot(const cfgpack_entry_t *entries, size_t entry_off) {
    int slot = 0;
    for (size_t i = 0; i < entry_off; ++i) {
        cfgpack_type_t t = entries[i].type;
        if (t == CFGPACK_TYPE_STR || t == CFGPACK_TYPE_FSTR) {
            slot++;
        }
    }
    cfgpack_type_t t = entries[entry_off].type;
    if (t != CFGPACK_TYPE_STR && t != CFGPACK_TYPE_FSTR) {
        return -1;
    }
    return slot;
}

/**
 * @brief Find entry position by index using binary search (sorted entries).
 */
static int find_entry_pos(const cfgpack_entry_t *entries, size_t count, uint16_t index) {
    size_t lo = 0;
    size_t hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (entries[mid].index == index) {
            return (int)mid;
        }
        if (entries[mid].index < index) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return -1;
}

/**
 * @brief Skip whitespace and return pointer to next non-space character.
 */
static const char *skip_space(const char *p) {
    while (*p && is_space((unsigned char)*p)) {
        p++;
    }
    return p;
}

/**
 * @brief Parse a quoted string default value into a fat value.
 */
static cfgpack_err_t parse_quoted_string(const char *p,
                                         cfgpack_fat_value_t *out,
                                         cfgpack_type_t type,
                                         const char **endp) {
    size_t max_len = (type == CFGPACK_TYPE_FSTR) ? CFGPACK_FSTR_MAX : CFGPACK_STR_MAX;
    char *dst = (type == CFGPACK_TYPE_FSTR) ? out->v.fstr.data : out->v.str.data;
    size_t len = 0;

    if (*p != '"') {
        return CFGPACK_ERR_PARSE;
    }
    p++; /* skip opening quote */

    while (*p && *p != '"') {
        if (len >= max_len) {
            return CFGPACK_ERR_STR_TOO_LONG;
        }
        if (*p == '\\' && *(p + 1)) {
            p++;
            switch (*p) {
            case 'n': dst[len++] = '\n'; break;
            case 't': dst[len++] = '\t'; break;
            case 'r': dst[len++] = '\r'; break;
            case '\\': dst[len++] = '\\'; break;
            case '"': dst[len++] = '"'; break;
            default: dst[len++] = *p; break;
            }
        } else {
            dst[len++] = *p;
        }
        p++;
    }

    if (*p != '"') {
        return CFGPACK_ERR_PARSE; /* unterminated string */
    }
    p++; /* skip closing quote */

    dst[len] = '\0';
    out->type = type;
    if (type == CFGPACK_TYPE_FSTR) {
        out->v.fstr.len = (uint8_t)len;
    } else {
        out->v.str.len = (uint16_t)len;
    }

    if (endp) {
        *endp = p;
    }
    return CFGPACK_OK;
}

/**
 * @brief Parse an unsigned integer default value.
 */
static cfgpack_err_t parse_uint(const char *tok, cfgpack_fat_value_t *out, cfgpack_type_t type) {
    char *endp = NULL;
    uint64_t val;
    uint64_t max_val;

    errno = 0;
    val = strtoull(tok, &endp, 0); /* base 0 handles 0x prefix */
    if (tok[0] == '\0' || (endp && *endp != '\0') || errno == ERANGE) {
        return CFGPACK_ERR_PARSE;
    }

    switch (type) {
    case CFGPACK_TYPE_U8: max_val = 0xFFULL; break;
    case CFGPACK_TYPE_U16: max_val = 0xFFFFULL; break;
    case CFGPACK_TYPE_U32: max_val = 0xFFFFFFFFULL; break;
    case CFGPACK_TYPE_U64: max_val = 0xFFFFFFFFFFFFFFFFULL; break;
    default: return CFGPACK_ERR_INVALID_TYPE;
    }

    if (val > max_val) {
        return CFGPACK_ERR_BOUNDS;
    }

    out->type = type;
    out->v.u64 = val;
    return CFGPACK_OK;
}

/**
 * @brief Parse a signed integer default value.
 */
static cfgpack_err_t parse_int(const char *tok, cfgpack_fat_value_t *out, cfgpack_type_t type) {
    char *endp = NULL;
    int64_t val;
    int64_t min_val, max_val;

    errno = 0;
    val = strtoll(tok, &endp, 0);
    if (tok[0] == '\0' || (endp && *endp != '\0') || errno == ERANGE) {
        return CFGPACK_ERR_PARSE;
    }

    switch (type) {
    case CFGPACK_TYPE_I8:
        min_val = -128;
        max_val = 127;
        break;
    case CFGPACK_TYPE_I16:
        min_val = -32768;
        max_val = 32767;
        break;
    case CFGPACK_TYPE_I32:
        min_val = -2147483648LL;
        max_val = 2147483647LL;
        break;
    case CFGPACK_TYPE_I64:
        min_val = INT64_MIN;
        max_val = INT64_MAX;
        break;
    default: return CFGPACK_ERR_INVALID_TYPE;
    }

    if (val < min_val || val > max_val) {
        return CFGPACK_ERR_BOUNDS;
    }

    out->type = type;
    out->v.i64 = val;
    return CFGPACK_OK;
}

/**
 * @brief Parse a float default value.
 */
static cfgpack_err_t parse_float(const char *tok, cfgpack_fat_value_t *out, cfgpack_type_t type) {
    char *endp = NULL;
    double val;

    errno = 0;
    val = strtod(tok, &endp);
    if (tok[0] == '\0' || (endp && *endp != '\0') || errno == ERANGE) {
        return CFGPACK_ERR_PARSE;
    }

    out->type = type;
    if (type == CFGPACK_TYPE_F32) {
        out->v.f32 = (float)val;
    } else {
        out->v.f64 = val;
    }
    return CFGPACK_OK;
}

/**
 * @brief Extract the default value token from a line (handles quoted strings).
 */
static const char *extract_default_token(const char *line, char *out_tok, size_t tok_size) {
    const char *p = skip_space(line);
    size_t len = 0;

    if (*p == '"') {
        /* Quoted string: copy until closing quote */
        out_tok[len++] = *p++;
        while (*p && *p != '"' && len < tok_size - 2) {
            if (*p == '\\' && *(p + 1)) {
                out_tok[len++] = *p++;
            }
            out_tok[len++] = *p++;
        }
        if (*p == '"' && len < tok_size - 1) {
            out_tok[len++] = *p++;
        }
        out_tok[len] = '\0';
    } else {
        /* Unquoted: copy until whitespace */
        while (*p && !is_space((unsigned char)*p) && len < tok_size - 1) {
            out_tok[len++] = *p++;
        }
        out_tok[len] = '\0';
    }

    return (len > 0) ? out_tok : NULL;
}

/**
 * @brief Parse a default value based on type.
 */
static cfgpack_err_t parse_default(const char *tok, cfgpack_type_t type, cfgpack_fat_value_t *out, uint8_t *has_def) {
    if (strcmp(tok, "NIL") == 0) {
        *has_def = 0;
        memset(out, 0, sizeof(*out));
        out->type = type;
        return CFGPACK_OK;
    }

    *has_def = 1;

    switch (type) {
    case CFGPACK_TYPE_U8:
    case CFGPACK_TYPE_U16:
    case CFGPACK_TYPE_U32:
    case CFGPACK_TYPE_U64: return parse_uint(tok, out, type);

    case CFGPACK_TYPE_I8:
    case CFGPACK_TYPE_I16:
    case CFGPACK_TYPE_I32:
    case CFGPACK_TYPE_I64: return parse_int(tok, out, type);

    case CFGPACK_TYPE_F32:
    case CFGPACK_TYPE_F64: return parse_float(tok, out, type);

    case CFGPACK_TYPE_STR:
    case CFGPACK_TYPE_FSTR: return parse_quoted_string(tok, out, type, NULL);
    }

    return CFGPACK_ERR_INVALID_TYPE;
}

/**
 * @brief Copy a scalar default from fat value into compact value.
 */
static void fat_to_compact_scalar(const cfgpack_fat_value_t *fat, cfgpack_value_t *out) {
    out->type = fat->type;
    switch (fat->type) {
    case CFGPACK_TYPE_U8:
    case CFGPACK_TYPE_U16:
    case CFGPACK_TYPE_U32:
    case CFGPACK_TYPE_U64: out->v.u64 = fat->v.u64; break;
    case CFGPACK_TYPE_I8:
    case CFGPACK_TYPE_I16:
    case CFGPACK_TYPE_I32:
    case CFGPACK_TYPE_I64: out->v.i64 = fat->v.i64; break;
    case CFGPACK_TYPE_F32: out->v.f32 = fat->v.f32; break;
    case CFGPACK_TYPE_F64: out->v.f64 = fat->v.f64; break;
    default: break;
    }
}

/**
 * @brief Write a fat string default into the string pool at the given position.
 */
static void fat_str_to_pool(const cfgpack_fat_value_t *fat,
                            size_t entry_pos,
                            cfgpack_value_t *values,
                            const cfgpack_entry_t *entries,
                            char *str_pool,
                            uint16_t *str_offsets) {
    int slot = get_str_slot(entries, entry_pos);
    if (slot < 0) {
        return;
    }

    uint16_t offset = str_offsets[slot];
    char *dst = str_pool + offset;

    if (fat->type == CFGPACK_TYPE_STR) {
        memcpy(dst, fat->v.str.data, fat->v.str.len);
        dst[fat->v.str.len] = '\0';
        values[entry_pos].type = CFGPACK_TYPE_STR;
        values[entry_pos].v.str.offset = offset;
        values[entry_pos].v.str.len = fat->v.str.len;
    } else if (fat->type == CFGPACK_TYPE_FSTR) {
        memcpy(dst, fat->v.fstr.data, fat->v.fstr.len);
        dst[fat->v.fstr.len] = '\0';
        values[entry_pos].type = CFGPACK_TYPE_FSTR;
        values[entry_pos].v.fstr.offset = offset;
        values[entry_pos].v.fstr.len = fat->v.fstr.len;
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * .map Schema Parser (buffer-based, two-phase for string defaults)
 * ───────────────────────────────────────────────────────────────────────────── */

cfgpack_err_t cfgpack_parse_schema(const char *data,
                                   size_t data_len,
                                   cfgpack_schema_t *out_schema,
                                   cfgpack_entry_t *entries,
                                   size_t max_entries,
                                   cfgpack_value_t *values,
                                   char *str_pool,
                                   size_t str_pool_cap,
                                   uint16_t *str_offsets,
                                   size_t str_offsets_count,
                                   cfgpack_parse_error_t *err) {
    line_iter_t iter;
    char line_buf[MAX_LINE_LEN];
    size_t line_no = 0;
    int header_read = 0;
    size_t count = 0;

    /* Zero values array up front */
    memset(values, 0, max_entries * sizeof(cfgpack_value_t));

    line_iter_init(&iter, data, data_len);

    const char *line;
    size_t line_len;

    /* ── Phase 1: Parse entries and scalar defaults ────────────────────── */
    while ((line = line_iter_next(&iter, &line_len)) != NULL) {
        char *slots[4];
        tokens_t tok;
        size_t stop_offset = 0;

        line_no++;

        /* Skip blank lines and comments */
        if (is_blank_or_comment_n(line, line_len)) {
            continue;
        }

        /* Copy line to mutable buffer (tokens_find modifies it) */
        if (line_len >= sizeof(line_buf)) {
            set_err(err, line_no, "line too long");
            return CFGPACK_ERR_PARSE;
        }
        memcpy(line_buf, line, line_len);
        line_buf[line_len] = '\0';

        if (tokens_create(&tok, 4, slots) != 0) {
            return CFGPACK_ERR_PARSE;
        }

        if (!header_read) {
            tokens_find(&tok, line_buf, " \t\r\n", 2, NULL);
            if (tok.used != 2) {
                set_err(err, line_no, "invalid header");
                tokens_destroy(&tok);
                return CFGPACK_ERR_PARSE;
            }
            if (strlen(tok.index[0]) >= sizeof(out_schema->map_name)) {
                set_err(err, line_no, "map name too long");
                tokens_destroy(&tok);
                return CFGPACK_ERR_BOUNDS;
            }
            char *endp = NULL;
            unsigned long ver = strtoul(tok.index[1], &endp, 10);
            if (tok.index[1][0] == '\0' || (endp && *endp != '\0')) {
                set_err(err, line_no, "invalid header");
                tokens_destroy(&tok);
                return CFGPACK_ERR_PARSE;
            }
            if (ver > 0xfffffffful) {
                set_err(err, line_no, "version out of range");
                tokens_destroy(&tok);
                return CFGPACK_ERR_BOUNDS;
            }
            size_t name_len = strlen(tok.index[0]);
            memcpy(out_schema->map_name, tok.index[0], name_len + 1);
            out_schema->version = (uint32_t)ver;
            tokens_destroy(&tok);
            header_read = 1;
            continue;
        }

        /* Parse first 3 tokens: index, name, type */
        tokens_find(&tok, line_buf, " \t\r\n", 3, &stop_offset);

        if (tok.used < 3) {
            set_err(err, line_no, "invalid entry");
            tokens_destroy(&tok);
            return CFGPACK_ERR_PARSE;
        }

        if (count >= max_entries) {
            set_err(err, line_no, "too many entries");
            tokens_destroy(&tok);
            return CFGPACK_ERR_BOUNDS;
        }

        unsigned long idx_ul = strtoul(tok.index[0], NULL, 10);
        if (idx_ul > 65535ul) {
            set_err(err, line_no, "index out of range");
            tokens_destroy(&tok);
            return CFGPACK_ERR_BOUNDS;
        }
        if (idx_ul == 0) {
            set_err(err, line_no, "index 0 is reserved for schema name");
            tokens_destroy(&tok);
            return CFGPACK_ERR_RESERVED_INDEX;
        }
        cfgpack_type_t type;
        cfgpack_err_t trc = parse_type(tok.index[2], &type);
        if (trc != CFGPACK_OK) {
            set_err(err, line_no, "invalid type");
            tokens_destroy(&tok);
            return trc;
        }
        if (name_too_long(tok.index[1])) {
            set_err(err, line_no, "name too long");
            tokens_destroy(&tok);
            return CFGPACK_ERR_BOUNDS;
        }
        if (has_duplicate(entries, count, (uint16_t)idx_ul, tok.index[1])) {
            set_err(err, line_no, "duplicate");
            tokens_destroy(&tok);
            return CFGPACK_ERR_DUPLICATE;
        }

        /* Extract default value from remainder of line */
        char default_tok[MAX_LINE_LEN];
        const char *remainder = line_buf + stop_offset;
        const char *def_str = extract_default_token(remainder, default_tok, sizeof(default_tok));
        if (!def_str || def_str[0] == '\0') {
            set_err(err, line_no, "missing default value");
            tokens_destroy(&tok);
            return CFGPACK_ERR_PARSE;
        }

        /* Parse default into stack-local fat value */
        cfgpack_fat_value_t fat_default;
        uint8_t has_def = 0;
        cfgpack_err_t drc = parse_default(def_str, type, &fat_default, &has_def);
        if (drc != CFGPACK_OK) {
            set_err(err, line_no, "invalid default value");
            tokens_destroy(&tok);
            return drc;
        }

        entries[count].index = (uint16_t)idx_ul;
        size_t name_len = strlen(tok.index[1]);
        memcpy(entries[count].name, tok.index[1], name_len + 1);
        entries[count].type = type;
        entries[count].has_default = has_def;

        /* Store scalar defaults directly into compact values */
        if (has_def && type != CFGPACK_TYPE_STR && type != CFGPACK_TYPE_FSTR) {
            fat_to_compact_scalar(&fat_default, &values[count]);
        } else {
            values[count].type = type;
        }

        count++;
        tokens_destroy(&tok);
    }

    if (!header_read) {
        set_err(err, 0, "missing header");
        return CFGPACK_ERR_PARSE;
    }

    /* Sort entries and values by index */
    sort_entries(entries, values, count);
    out_schema->entries = entries;
    out_schema->entry_count = count;

    /* Compute string pool offsets in sorted order */
    size_t pool_needed = compute_str_offsets(entries, count, str_offsets, str_offsets_count);
    if (pool_needed > str_pool_cap) {
        set_err(err, 0, "string pool too small");
        return CFGPACK_ERR_BOUNDS;
    }
    if (str_pool_cap > 0) {
        memset(str_pool, 0, str_pool_cap);
    }

    /* ── Phase 2: Re-parse input to extract string defaults ───────────── */
    line_iter_init(&iter, data, data_len);
    line_no = 0;
    header_read = 0;

    while ((line = line_iter_next(&iter, &line_len)) != NULL) {
        char *slots[4];
        tokens_t tok;
        size_t stop_offset = 0;

        line_no++;
        if (is_blank_or_comment_n(line, line_len)) {
            continue;
        }

        if (line_len >= sizeof(line_buf)) {
            continue; /* already validated */
        }
        memcpy(line_buf, line, line_len);
        line_buf[line_len] = '\0';

        if (tokens_create(&tok, 4, slots) != 0) {
            continue;
        }

        if (!header_read) {
            tokens_destroy(&tok);
            header_read = 1;
            continue;
        }

        tokens_find(&tok, line_buf, " \t\r\n", 3, &stop_offset);
        if (tok.used < 3) {
            tokens_destroy(&tok);
            continue;
        }

        unsigned long idx_ul = strtoul(tok.index[0], NULL, 10);
        cfgpack_type_t type;
        parse_type(tok.index[2], &type);

        /* Only process string entries with defaults */
        if (type != CFGPACK_TYPE_STR && type != CFGPACK_TYPE_FSTR) {
            tokens_destroy(&tok);
            continue;
        }

        char default_tok[MAX_LINE_LEN];
        const char *remainder = line_buf + stop_offset;
        const char *def_str = extract_default_token(remainder, default_tok, sizeof(default_tok));
        if (!def_str || strcmp(def_str, "NIL") == 0) {
            tokens_destroy(&tok);
            continue;
        }

        /* Parse string default into stack-local fat value */
        cfgpack_fat_value_t fat_default;
        uint8_t has_def = 0;
        parse_default(def_str, type, &fat_default, &has_def);

        if (has_def) {
            /* Find this entry's position in the sorted array */
            int pos = find_entry_pos(entries, count, (uint16_t)idx_ul);
            if (pos >= 0) {
                fat_str_to_pool(&fat_default, (size_t)pos, values, entries, str_pool, str_offsets);
            }
        }

        tokens_destroy(&tok);
    }

    return CFGPACK_OK;
}

void cfgpack_schema_free(cfgpack_schema_t *schema) {
    (void)schema; /* no-op: caller owns buffers */
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Type to String Helper
 * ───────────────────────────────────────────────────────────────────────────── */

static const char *type_to_str(cfgpack_type_t type) {
    switch (type) {
    case CFGPACK_TYPE_U8: return "u8";
    case CFGPACK_TYPE_U16: return "u16";
    case CFGPACK_TYPE_U32: return "u32";
    case CFGPACK_TYPE_U64: return "u64";
    case CFGPACK_TYPE_I8: return "i8";
    case CFGPACK_TYPE_I16: return "i16";
    case CFGPACK_TYPE_I32: return "i32";
    case CFGPACK_TYPE_I64: return "i64";
    case CFGPACK_TYPE_F32: return "f32";
    case CFGPACK_TYPE_F64: return "f64";
    case CFGPACK_TYPE_STR: return "str";
    case CFGPACK_TYPE_FSTR: return "fstr";
    }
    return "unknown";
}

/* ─────────────────────────────────────────────────────────────────────────────
 * JSON Writer (buffer-based) - reads from cfgpack_ctx_t
 * ───────────────────────────────────────────────────────────────────────────── */

static void write_json_string_to_wbuf(wbuf_t *w, const char *str, size_t len) {
    wbuf_putc(w, '"');
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)str[i];
        switch (c) {
        case '"': wbuf_puts(w, "\\\""); break;
        case '\\': wbuf_puts(w, "\\\\"); break;
        case '\n': wbuf_puts(w, "\\n"); break;
        case '\r': wbuf_puts(w, "\\r"); break;
        case '\t': wbuf_puts(w, "\\t"); break;
        default:
            if (c < 0x20) {
                /* \uXXXX escape */
                char hex[7];
                hex[0] = '\\';
                hex[1] = 'u';
                hex[2] = '0';
                hex[3] = '0';
                hex[4] = "0123456789abcdef"[(c >> 4) & 0xf];
                hex[5] = "0123456789abcdef"[c & 0xf];
                hex[6] = '\0';
                wbuf_puts(w, hex);
            } else {
                wbuf_putc(w, (char)c);
            }
            break;
        }
    }
    wbuf_putc(w, '"');
}

static void write_json_value_to_wbuf(wbuf_t *w,
                                     const cfgpack_entry_t *entry,
                                     const cfgpack_value_t *val,
                                     const char *str_pool) {
    if (!entry->has_default) {
        wbuf_puts(w, "null");
        return;
    }

    switch (entry->type) {
    case CFGPACK_TYPE_U8:
    case CFGPACK_TYPE_U16:
    case CFGPACK_TYPE_U32:
    case CFGPACK_TYPE_U64: wbuf_put_uint(w, val->v.u64); break;
    case CFGPACK_TYPE_I8:
    case CFGPACK_TYPE_I16:
    case CFGPACK_TYPE_I32:
    case CFGPACK_TYPE_I64: wbuf_put_int(w, val->v.i64); break;
    case CFGPACK_TYPE_F32: wbuf_put_float(w, val->v.f32); break;
    case CFGPACK_TYPE_F64: wbuf_put_double(w, val->v.f64); break;
    case CFGPACK_TYPE_STR: write_json_string_to_wbuf(w, str_pool + val->v.str.offset, val->v.str.len); break;
    case CFGPACK_TYPE_FSTR: write_json_string_to_wbuf(w, str_pool + val->v.fstr.offset, val->v.fstr.len); break;
    }
}

cfgpack_err_t cfgpack_schema_write_json(const cfgpack_ctx_t *ctx,
                                        char *out,
                                        size_t out_cap,
                                        size_t *out_len,
                                        cfgpack_parse_error_t *err) {
    const cfgpack_schema_t *schema = ctx->schema;
    wbuf_t w;
    wbuf_init(&w, out, out_cap);

    wbuf_puts(&w, "{\n");
    wbuf_puts(&w, "  \"name\": \"");
    wbuf_puts(&w, schema->map_name);
    wbuf_puts(&w, "\",\n");
    wbuf_puts(&w, "  \"version\": ");
    wbuf_put_uint(&w, schema->version);
    wbuf_puts(&w, ",\n");
    wbuf_puts(&w, "  \"entries\": [\n");

    for (size_t i = 0; i < schema->entry_count; ++i) {
        const cfgpack_entry_t *e = &schema->entries[i];
        wbuf_puts(&w, "    {\"index\": ");
        wbuf_put_uint(&w, e->index);
        wbuf_puts(&w, ", \"name\": \"");
        wbuf_puts(&w, e->name);
        wbuf_puts(&w, "\", \"type\": \"");
        wbuf_puts(&w, type_to_str(e->type));
        wbuf_puts(&w, "\", \"value\": ");
        write_json_value_to_wbuf(&w, e, &ctx->values[i], ctx->str_pool);
        if (i + 1 < schema->entry_count) {
            wbuf_puts(&w, "},\n");
        } else {
            wbuf_puts(&w, "}\n");
        }
    }

    wbuf_puts(&w, "  ]\n");
    wbuf_puts(&w, "}\n");

    if (out_len) {
        *out_len = w.len;
    }

    if (w.len > out_cap) {
        set_err(err, 0, "buffer too small");
        return CFGPACK_ERR_BOUNDS;
    }

    (void)err;
    return CFGPACK_OK;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * JSON Parser (buffer-based, no malloc)
 * ───────────────────────────────────────────────────────────────────────────── */

typedef struct {
    const char *data;
    size_t len;
    size_t pos;
    size_t line;
} json_parser_t;

static void json_skip_ws(json_parser_t *p) {
    while (p->pos < p->len) {
        char c = p->data[p->pos];
        if (c == '\n') {
            p->line++;
            p->pos++;
        } else if (c == ' ' || c == '\t' || c == '\r') {
            p->pos++;
        } else {
            break;
        }
    }
}

static char json_peek(json_parser_t *p) {
    json_skip_ws(p);
    return (p->pos < p->len) ? p->data[p->pos] : '\0';
}

static int json_expect(json_parser_t *p, char c) {
    json_skip_ws(p);
    if (p->pos < p->len && p->data[p->pos] == c) {
        if (c == '\n') {
            p->line++;
        }
        p->pos++;
        return 1;
    }
    return 0;
}

static int json_parse_string(json_parser_t *p, char *out, size_t out_cap, size_t *out_len) {
    json_skip_ws(p);
    if (!json_expect(p, '"')) {
        return 0;
    }

    size_t len = 0;
    while (p->pos < p->len && p->data[p->pos] != '"') {
        if (len >= out_cap - 1) {
            return 0; /* overflow */
        }

        if (p->data[p->pos] == '\\' && p->pos + 1 < p->len) {
            p->pos++;
            switch (p->data[p->pos]) {
            case 'n': out[len++] = '\n'; break;
            case 't': out[len++] = '\t'; break;
            case 'r': out[len++] = '\r'; break;
            case '\\': out[len++] = '\\'; break;
            case '"': out[len++] = '"'; break;
            case 'u':
                /* \uXXXX - parse 4 hex digits */
                if (p->pos + 4 < p->len) {
                    char hex[5] = {p->data[p->pos + 1],
                                   p->data[p->pos + 2],
                                   p->data[p->pos + 3],
                                   p->data[p->pos + 4],
                                   '\0'};
                    unsigned int val = 0;
                    int ok = 1;
                    for (int i = 0; i < 4; i++) {
                        char h = hex[i];
                        if (h >= '0' && h <= '9') {
                            val = val * 16 + (h - '0');
                        } else if (h >= 'a' && h <= 'f') {
                            val = val * 16 + (h - 'a' + 10);
                        } else if (h >= 'A' && h <= 'F') {
                            val = val * 16 + (h - 'A' + 10);
                        } else {
                            ok = 0;
                            break;
                        }
                    }
                    if (ok && val < 256) {
                        out[len++] = (char)val;
                    }
                    p->pos += 4;
                }
                break;
            default: out[len++] = p->data[p->pos]; break;
            }
        } else {
            out[len++] = p->data[p->pos];
        }
        p->pos++;
    }

    if (!json_expect(p, '"')) {
        return 0;
    }
    out[len] = '\0';
    if (out_len) {
        *out_len = len;
    }
    return 1;
}

static int json_parse_number(json_parser_t *p, int64_t *out_int, double *out_float, int *is_float) {
    json_skip_ws(p);
    size_t start = p->pos;
    int has_dot = 0, has_exp = 0;

    /* Optional minus */
    if (p->pos < p->len && p->data[p->pos] == '-') {
        p->pos++;
    }

    /* Digits */
    while (p->pos < p->len && (p->data[p->pos] >= '0' && p->data[p->pos] <= '9')) {
        p->pos++;
    }

    /* Optional decimal */
    if (p->pos < p->len && p->data[p->pos] == '.') {
        has_dot = 1;
        p->pos++;
        while (p->pos < p->len && (p->data[p->pos] >= '0' && p->data[p->pos] <= '9')) {
            p->pos++;
        }
    }

    /* Optional exponent */
    if (p->pos < p->len && (p->data[p->pos] == 'e' || p->data[p->pos] == 'E')) {
        has_exp = 1;
        p->pos++;
        if (p->pos < p->len && (p->data[p->pos] == '+' || p->data[p->pos] == '-')) {
            p->pos++;
        }
        while (p->pos < p->len && (p->data[p->pos] >= '0' && p->data[p->pos] <= '9')) {
            p->pos++;
        }
    }

    if (p->pos == start) {
        return 0;
    }

    /* Parse the number */
    char buf[64];
    size_t num_len = p->pos - start;
    if (num_len >= sizeof(buf)) {
        return 0;
    }
    memcpy(buf, p->data + start, num_len);
    buf[num_len] = '\0';

    *is_float = has_dot || has_exp;
    if (*is_float) {
        *out_float = strtod(buf, NULL);
    } else {
        *out_int = strtoll(buf, NULL, 10);
    }
    return 1;
}

static int json_match_literal(json_parser_t *p, const char *lit) {
    json_skip_ws(p);
    size_t lit_len = strlen(lit);
    if (p->pos + lit_len <= p->len && memcmp(p->data + p->pos, lit, lit_len) == 0) {
        p->pos += lit_len;
        return 1;
    }
    return 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Schema Sizing
 * ───────────────────────────────────────────────────────────────────────────── */

cfgpack_err_t cfgpack_schema_get_sizing(const cfgpack_schema_t *schema, cfgpack_schema_sizing_t *out) {
    size_t str_count = 0;
    size_t fstr_count = 0;
    size_t str_pool_size = 0;

    for (size_t i = 0; i < schema->entry_count; ++i) {
        switch (schema->entries[i].type) {
        case CFGPACK_TYPE_STR:
            str_count++;
            str_pool_size += CFGPACK_STR_MAX + 1; /* Fixed slot per entry */
            break;
        case CFGPACK_TYPE_FSTR:
            fstr_count++;
            str_pool_size += CFGPACK_FSTR_MAX + 1; /* Fixed slot per entry */
            break;
        default: break;
        }
    }

    out->str_count = str_count;
    out->fstr_count = fstr_count;
    out->str_pool_size = str_pool_size;

    return CFGPACK_OK;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * JSON Schema Parser (buffer-based, no malloc, two-phase string defaults)
 * ───────────────────────────────────────────────────────────────────────────── */

cfgpack_err_t cfgpack_schema_parse_json(const char *data,
                                        size_t data_len,
                                        cfgpack_schema_t *out_schema,
                                        cfgpack_entry_t *entries,
                                        size_t max_entries,
                                        cfgpack_value_t *values,
                                        char *str_pool,
                                        size_t str_pool_cap,
                                        uint16_t *str_offsets,
                                        size_t str_offsets_count,
                                        cfgpack_parse_error_t *err) {
    json_parser_t parser = {data, data_len, 0, 1};
    json_parser_t *p = &parser;
    size_t count = 0;

    /* Zero values array up front */
    memset(values, 0, max_entries * sizeof(cfgpack_value_t));

    /* Expect top-level object */
    if (!json_expect(p, '{')) {
        set_err(err, p->line, "expected '{'");
        return CFGPACK_ERR_PARSE;
    }

    /* Parse top-level keys: name, version, entries */
    int got_name = 0, got_version = 0, got_entries = 0;

    while (json_peek(p) != '}' && json_peek(p) != '\0') {
        char key[32];
        size_t key_len;
        if (!json_parse_string(p, key, sizeof(key), &key_len)) {
            set_err(err, p->line, "expected key");
            return CFGPACK_ERR_PARSE;
        }
        if (!json_expect(p, ':')) {
            set_err(err, p->line, "expected ':'");
            return CFGPACK_ERR_PARSE;
        }

        if (strcmp(key, "name") == 0) {
            if (!json_parse_string(p, out_schema->map_name, sizeof(out_schema->map_name), NULL)) {
                set_err(err, p->line, "invalid name");
                return CFGPACK_ERR_PARSE;
            }
            got_name = 1;
        } else if (strcmp(key, "version") == 0) {
            int64_t ver;
            double ver_f;
            int is_float;
            if (!json_parse_number(p, &ver, &ver_f, &is_float) || is_float || ver < 0) {
                set_err(err, p->line, "invalid version");
                return CFGPACK_ERR_PARSE;
            }
            out_schema->version = (uint32_t)ver;
            got_version = 1;
        } else if (strcmp(key, "entries") == 0) {
            if (!json_expect(p, '[')) {
                set_err(err, p->line, "expected '['");
                return CFGPACK_ERR_PARSE;
            }

            while (json_peek(p) != ']' && json_peek(p) != '\0') {
                if (count >= max_entries) {
                    set_err(err, p->line, "too many entries");
                    return CFGPACK_ERR_BOUNDS;
                }

                if (!json_expect(p, '{')) {
                    set_err(err, p->line, "expected '{'");
                    return CFGPACK_ERR_PARSE;
                }

                cfgpack_entry_t *e = &entries[count];
                memset(e, 0, sizeof(*e));

                int got_index = 0, got_ename = 0, got_type = 0, got_default = 0;

                /* Temporary storage for default value (parsed after we know the type) */
                int default_is_null = 0;
                int default_is_string = 0;
                int default_is_number = 0;
                char default_str[CFGPACK_STR_MAX + 1];
                size_t default_str_len = 0;
                int64_t default_ival = 0;
                double default_fval = 0;
                int default_is_float = 0;

                while (json_peek(p) != '}' && json_peek(p) != '\0') {
                    char ekey[32];
                    size_t ekey_len;
                    if (!json_parse_string(p, ekey, sizeof(ekey), &ekey_len)) {
                        set_err(err, p->line, "expected entry key");
                        return CFGPACK_ERR_PARSE;
                    }
                    if (!json_expect(p, ':')) {
                        set_err(err, p->line, "expected ':'");
                        return CFGPACK_ERR_PARSE;
                    }

                    if (strcmp(ekey, "index") == 0) {
                        int64_t idx;
                        double idx_f;
                        int is_float;
                        if (!json_parse_number(p, &idx, &idx_f, &is_float) || is_float || idx < 0 || idx > 65535) {
                            set_err(err, p->line, "invalid index");
                            return CFGPACK_ERR_BOUNDS;
                        }
                        e->index = (uint16_t)idx;
                        got_index = 1;
                    } else if (strcmp(ekey, "name") == 0) {
                        char name_buf[32];
                        if (!json_parse_string(p, name_buf, sizeof(name_buf), NULL)) {
                            set_err(err, p->line, "invalid entry name");
                            return CFGPACK_ERR_PARSE;
                        }
                        if (name_too_long(name_buf)) {
                            set_err(err, p->line, "name too long");
                            return CFGPACK_ERR_BOUNDS;
                        }
                        size_t name_len = strlen(name_buf);
                        memcpy(e->name, name_buf, name_len + 1);
                        got_ename = 1;
                    } else if (strcmp(ekey, "type") == 0) {
                        char type_buf[16];
                        if (!json_parse_string(p, type_buf, sizeof(type_buf), NULL)) {
                            set_err(err, p->line, "invalid type");
                            return CFGPACK_ERR_PARSE;
                        }
                        cfgpack_err_t trc = parse_type(type_buf, &e->type);
                        if (trc != CFGPACK_OK) {
                            set_err(err, p->line, "invalid type");
                            return trc;
                        }
                        got_type = 1;
                    } else if (strcmp(ekey, "value") == 0) {
                        char c = json_peek(p);
                        if (json_match_literal(p, "null")) {
                            default_is_null = 1;
                        } else if (c == '"') {
                            /* String default - parse into temp buffer */
                            if (!json_parse_string(p, default_str, sizeof(default_str), &default_str_len)) {
                                set_err(err, p->line, "invalid string default");
                                return CFGPACK_ERR_PARSE;
                            }
                            default_is_string = 1;
                        } else {
                            /* Numeric default */
                            if (!json_parse_number(p, &default_ival, &default_fval, &default_is_float)) {
                                set_err(err, p->line, "invalid default value");
                                return CFGPACK_ERR_PARSE;
                            }
                            default_is_number = 1;
                        }
                        got_default = 1;
                    } else {
                        /* Skip unknown key value */
                        set_err(err, p->line, "unknown entry key");
                        return CFGPACK_ERR_PARSE;
                    }

                    /* Skip comma if present */
                    json_expect(p, ',');
                }

                if (!json_expect(p, '}')) {
                    set_err(err, p->line, "expected '}'");
                    return CFGPACK_ERR_PARSE;
                }

                if (!got_index || !got_ename || !got_type || !got_default) {
                    set_err(err, p->line, "missing entry field");
                    return CFGPACK_ERR_PARSE;
                }

                if (e->index == 0) {
                    set_err(err, p->line, "index 0 is reserved for schema name");
                    return CFGPACK_ERR_RESERVED_INDEX;
                }

                /* Assign default values into compact values */
                if (default_is_null) {
                    e->has_default = 0;
                    values[count].type = e->type;
                } else if (default_is_string) {
                    e->has_default = 1;
                    values[count].type = e->type;
                    /* String data will be written to pool in phase 2 (after sort).
                     * Store the string temporarily: we'll re-parse from the JSON
                     * input. But since JSON entries come with all data inline,
                     * we can build a fat value now and apply after sorting. */

                    /* For JSON, we have the string data right here in default_str.
                     * We'll mark has_default and store type. The actual pool write
                     * happens below after sorting. We need to stash the string data.
                     * Since we can't store it in the values array (it only has offsets),
                     * we'll do a second pass below. But for JSON we have a simpler
                     * approach: we process entries after sorting using the same
                     * technique as .map: re-parse the JSON. */
                    if (e->type == CFGPACK_TYPE_FSTR) {
                        if (default_str_len > CFGPACK_FSTR_MAX) {
                            set_err(err, p->line, "fstr too long");
                            return CFGPACK_ERR_STR_TOO_LONG;
                        }
                    } else {
                        if (default_str_len > CFGPACK_STR_MAX) {
                            set_err(err, p->line, "str too long");
                            return CFGPACK_ERR_STR_TOO_LONG;
                        }
                    }
                } else if (default_is_number) {
                    e->has_default = 1;
                    values[count].type = e->type;
                    switch (e->type) {
                    case CFGPACK_TYPE_U8:
                    case CFGPACK_TYPE_U16:
                    case CFGPACK_TYPE_U32:
                    case CFGPACK_TYPE_U64: values[count].v.u64 = (uint64_t)default_ival; break;
                    case CFGPACK_TYPE_I8:
                    case CFGPACK_TYPE_I16:
                    case CFGPACK_TYPE_I32:
                    case CFGPACK_TYPE_I64: values[count].v.i64 = default_ival; break;
                    case CFGPACK_TYPE_F32:
                        values[count].v.f32 = default_is_float ? (float)default_fval : (float)default_ival;
                        break;
                    case CFGPACK_TYPE_F64:
                        values[count].v.f64 = default_is_float ? default_fval : (double)default_ival;
                        break;
                    default: break;
                    }
                }

                /* Check for duplicates */
                if (has_duplicate(entries, count, e->index, e->name)) {
                    set_err(err, p->line, "duplicate");
                    return CFGPACK_ERR_DUPLICATE;
                }

                count++;

                /* Skip comma if present */
                json_expect(p, ',');
            }

            if (!json_expect(p, ']')) {
                set_err(err, p->line, "expected ']'");
                return CFGPACK_ERR_PARSE;
            }
            got_entries = 1;
        } else {
            /* Skip unknown top-level key */
            set_err(err, p->line, "unknown key");
            return CFGPACK_ERR_PARSE;
        }

        /* Skip comma if present */
        json_expect(p, ',');
    }

    if (!json_expect(p, '}')) {
        set_err(err, p->line, "expected '}'");
        return CFGPACK_ERR_PARSE;
    }

    if (!got_name || !got_version || !got_entries) {
        set_err(err, p->line, "missing required field");
        return CFGPACK_ERR_PARSE;
    }

    /* Sort entries and values by index */
    sort_entries(entries, values, count);
    out_schema->entries = entries;
    out_schema->entry_count = count;

    /* Compute string pool offsets in sorted order */
    size_t pool_needed = compute_str_offsets(entries, count, str_offsets, str_offsets_count);
    if (pool_needed > str_pool_cap) {
        set_err(err, 0, "string pool too small");
        return CFGPACK_ERR_BOUNDS;
    }
    if (str_pool_cap > 0) {
        memset(str_pool, 0, str_pool_cap);
    }

    /* ── Phase 2: Re-parse JSON to extract string defaults ────────────── */
    {
        json_parser_t p2 = {data, data_len, 0, 1};
        json_parser_t *jp = &p2;

        if (!json_expect(jp, '{')) {
            return CFGPACK_OK; /* should not happen */
        }

        while (json_peek(jp) != '}' && json_peek(jp) != '\0') {
            char key[32];
            size_t key_len;
            if (!json_parse_string(jp, key, sizeof(key), &key_len)) {
                break;
            }
            if (!json_expect(jp, ':')) {
                break;
            }

            if (strcmp(key, "entries") == 0) {
                if (!json_expect(jp, '[')) {
                    break;
                }

                while (json_peek(jp) != ']' && json_peek(jp) != '\0') {
                    if (!json_expect(jp, '{')) {
                        break;
                    }

                    uint16_t entry_index = 0;
                    cfgpack_type_t entry_type = CFGPACK_TYPE_U8;
                    int has_string_default = 0;
                    char str_data[CFGPACK_STR_MAX + 1];
                    size_t str_len = 0;
                    int entry_has_default = 0;

                    while (json_peek(jp) != '}' && json_peek(jp) != '\0') {
                        char ekey[32];
                        size_t ekey_len;
                        if (!json_parse_string(jp, ekey, sizeof(ekey), &ekey_len)) {
                            break;
                        }
                        if (!json_expect(jp, ':')) {
                            break;
                        }

                        if (strcmp(ekey, "index") == 0) {
                            int64_t idx;
                            double idx_f;
                            int is_float;
                            json_parse_number(jp, &idx, &idx_f, &is_float);
                            entry_index = (uint16_t)idx;
                        } else if (strcmp(ekey, "name") == 0) {
                            char tmp[32];
                            json_parse_string(jp, tmp, sizeof(tmp), NULL);
                        } else if (strcmp(ekey, "type") == 0) {
                            char type_buf[16];
                            json_parse_string(jp, type_buf, sizeof(type_buf), NULL);
                            parse_type(type_buf, &entry_type);
                        } else if (strcmp(ekey, "value") == 0) {
                            char c = json_peek(jp);
                            if (json_match_literal(jp, "null")) {
                                /* no default */
                            } else if (c == '"') {
                                if (json_parse_string(jp, str_data, sizeof(str_data), &str_len)) {
                                    has_string_default = 1;
                                    entry_has_default = 1;
                                }
                            } else {
                                int64_t iv;
                                double fv;
                                int isf;
                                json_parse_number(jp, &iv, &fv, &isf);
                                entry_has_default = 1;
                            }
                        }
                        json_expect(jp, ',');
                    }
                    json_expect(jp, '}');

                    /* Write string default to pool if applicable */
                    if (has_string_default && entry_has_default &&
                        (entry_type == CFGPACK_TYPE_STR || entry_type == CFGPACK_TYPE_FSTR)) {
                        int pos = find_entry_pos(entries, count, entry_index);
                        if (pos >= 0) {
                            cfgpack_fat_value_t fat;
                            memset(&fat, 0, sizeof(fat));
                            fat.type = entry_type;
                            if (entry_type == CFGPACK_TYPE_FSTR) {
                                fat.v.fstr.len = (uint8_t)str_len;
                                memcpy(fat.v.fstr.data, str_data, str_len + 1);
                            } else {
                                fat.v.str.len = (uint16_t)str_len;
                                memcpy(fat.v.str.data, str_data, str_len + 1);
                            }
                            fat_str_to_pool(&fat, (size_t)pos, values, entries, str_pool, str_offsets);
                        }
                    }

                    json_expect(jp, ',');
                }
                json_expect(jp, ']');
            } else if (strcmp(key, "name") == 0) {
                char tmp[64];
                json_parse_string(jp, tmp, sizeof(tmp), NULL);
            } else if (strcmp(key, "version") == 0) {
                int64_t v;
                double vf;
                int isf;
                json_parse_number(jp, &v, &vf, &isf);
            }
            json_expect(jp, ',');
        }
    }

    return CFGPACK_OK;
}
