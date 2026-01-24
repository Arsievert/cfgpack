#include "cfgpack/schema.h"
#include "cfgpack/error.h"
#include "cfgpack/value.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

#include "tokens.h"

#define MAX_LINE_LEN 256

/**
 * @brief Populate a parse error structure.
 * @param err   Error structure to fill (may be NULL).
 * @param line  Line number where the error occurred.
 * @param msg   Human-readable error message.
 */
static void set_err(cfgpack_parse_error_t *err, size_t line, const char *msg) {
    if (!err) return;
    err->line = line;
    snprintf(err->message, sizeof(err->message), "%s", msg);
}

/**
 * @brief Check if a character is whitespace.
 * @param c  Character to test.
 * @return Non-zero if whitespace, zero otherwise.
 */
static int is_space(unsigned char c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r');
}

/**
 * @brief Check if a line is blank or a comment.
 * @param line  Null-terminated line string.
 * @return Non-zero if line is empty/whitespace or starts with '#'.
 */
static int is_blank_or_comment(const char *line) {
    while (*line && is_space((unsigned char)*line)) line++;
    return (*line == '\0' || *line == '#');
}


/**
 * @brief Parse a type string into a cfgpack_type_t.
 * @param tok  Type token (e.g. "u8", "i32", "str").
 * @param out  Output type enum.
 * @return CFGPACK_OK on success, CFGPACK_ERR_INVALID_TYPE if unrecognized.
 */
static cfgpack_err_t parse_type(const char *tok, cfgpack_type_t *out) {
    if (strcmp(tok, "u8") == 0) { *out = CFGPACK_TYPE_U8; return CFGPACK_OK; }
    if (strcmp(tok, "u16") == 0) { *out = CFGPACK_TYPE_U16; return CFGPACK_OK; }
    if (strcmp(tok, "u32") == 0) { *out = CFGPACK_TYPE_U32; return CFGPACK_OK; }
    if (strcmp(tok, "u64") == 0) { *out = CFGPACK_TYPE_U64; return CFGPACK_OK; }
    if (strcmp(tok, "i8") == 0) { *out = CFGPACK_TYPE_I8; return CFGPACK_OK; }
    if (strcmp(tok, "i16") == 0) { *out = CFGPACK_TYPE_I16; return CFGPACK_OK; }
    if (strcmp(tok, "i32") == 0) { *out = CFGPACK_TYPE_I32; return CFGPACK_OK; }
    if (strcmp(tok, "i64") == 0) { *out = CFGPACK_TYPE_I64; return CFGPACK_OK; }
    if (strcmp(tok, "f32") == 0) { *out = CFGPACK_TYPE_F32; return CFGPACK_OK; }
    if (strcmp(tok, "f64") == 0) { *out = CFGPACK_TYPE_F64; return CFGPACK_OK; }
    if (strcmp(tok, "str") == 0) { *out = CFGPACK_TYPE_STR; return CFGPACK_OK; }
    if (strcmp(tok, "fstr") == 0) { *out = CFGPACK_TYPE_FSTR; return CFGPACK_OK; }
    return CFGPACK_ERR_INVALID_TYPE;
}

/**
 * @brief Check if an entry name exceeds the allowed length.
 * @param name  Null-terminated name string.
 * @return Non-zero if name is empty or longer than 5 characters.
 */
static int name_too_long(const char *name) {
    return strlen(name) > 5 || strlen(name) == 0;
}

/**
 * @brief Check for duplicate index or name in existing entries.
 * @param entries  Array of parsed entries.
 * @param count    Number of entries in array.
 * @param idx      Index to check for duplicates.
 * @param name     Name to check for duplicates.
 * @return Non-zero if a duplicate index or name exists.
 */
static int has_duplicate(const cfgpack_entry_t *entries, size_t count, uint16_t idx, const char *name) {
    for (size_t i = 0; i < count; ++i) {
        if (entries[i].index == idx) return 1;
        if (strcmp(entries[i].name, name) == 0) return 1;
    }
    return 0;
}

/**
 * @brief Sort entries and defaults by index using insertion sort.
 * @param entries   Array of entries to sort in-place.
 * @param defaults  Parallel array of defaults to sort in-place.
 * @param n         Number of entries.
 */
static void sort_entries(cfgpack_entry_t *entries, cfgpack_value_t *defaults, size_t n) {
    for (size_t i = 1; i < n; ++i) {
        cfgpack_entry_t key_entry = entries[i];
        cfgpack_value_t key_default = defaults[i];
        size_t j = i;
        while (j > 0 && entries[j - 1].index > key_entry.index) {
            entries[j] = entries[j - 1];
            defaults[j] = defaults[j - 1];
            --j;
        }
        entries[j] = key_entry;
        defaults[j] = key_default;
    }
}

/**
 * @brief Skip whitespace and return pointer to next non-space character.
 * @param p  Input pointer.
 * @return Pointer to first non-whitespace character.
 */
static const char *skip_space(const char *p) {
    while (*p && is_space((unsigned char)*p)) p++;
    return p;
}

/**
 * @brief Parse a quoted string default value.
 * @param p     Pointer to opening quote.
 * @param out   Output value structure.
 * @param type  Expected type (CFGPACK_TYPE_STR or CFGPACK_TYPE_FSTR).
 * @param endp  Output pointer to character after closing quote.
 * @return CFGPACK_OK on success, error code on failure.
 */
static cfgpack_err_t parse_quoted_string(const char *p, cfgpack_value_t *out, cfgpack_type_t type, const char **endp) {
    size_t max_len = (type == CFGPACK_TYPE_FSTR) ? CFGPACK_FSTR_MAX : CFGPACK_STR_MAX;
    char *dst = (type == CFGPACK_TYPE_FSTR) ? out->v.fstr.data : out->v.str.data;
    size_t len = 0;

    if (*p != '"') return CFGPACK_ERR_PARSE;
    p++; /* skip opening quote */

    while (*p && *p != '"') {
        if (len >= max_len) return CFGPACK_ERR_STR_TOO_LONG;
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

    if (*p != '"') return CFGPACK_ERR_PARSE; /* unterminated string */
    p++; /* skip closing quote */

    dst[len] = '\0';
    out->type = type;
    if (type == CFGPACK_TYPE_FSTR) {
        out->v.fstr.len = (uint8_t)len;
    } else {
        out->v.str.len = (uint16_t)len;
    }

    if (endp) *endp = p;
    return CFGPACK_OK;
}

/**
 * @brief Parse an unsigned integer default value.
 * @param tok   Token string.
 * @param out   Output value structure.
 * @param type  Expected unsigned integer type.
 * @return CFGPACK_OK on success, error code on failure.
 */
static cfgpack_err_t parse_uint(const char *tok, cfgpack_value_t *out, cfgpack_type_t type) {
    char *endp = NULL;
    uint64_t val;
    uint64_t max_val;

    errno = 0;
    val = strtoull(tok, &endp, 0); /* base 0 handles 0x prefix */
    if (tok[0] == '\0' || (endp && *endp != '\0') || errno == ERANGE) {
        return CFGPACK_ERR_PARSE;
    }

    switch (type) {
        case CFGPACK_TYPE_U8:  max_val = 0xFFULL; break;
        case CFGPACK_TYPE_U16: max_val = 0xFFFFULL; break;
        case CFGPACK_TYPE_U32: max_val = 0xFFFFFFFFULL; break;
        case CFGPACK_TYPE_U64: max_val = 0xFFFFFFFFFFFFFFFFULL; break;
        default: return CFGPACK_ERR_INVALID_TYPE;
    }

    if (val > max_val) return CFGPACK_ERR_BOUNDS;

    out->type = type;
    out->v.u64 = val;
    return CFGPACK_OK;
}

/**
 * @brief Parse a signed integer default value.
 * @param tok   Token string.
 * @param out   Output value structure.
 * @param type  Expected signed integer type.
 * @return CFGPACK_OK on success, error code on failure.
 */
static cfgpack_err_t parse_int(const char *tok, cfgpack_value_t *out, cfgpack_type_t type) {
    char *endp = NULL;
    int64_t val;
    int64_t min_val, max_val;

    errno = 0;
    val = strtoll(tok, &endp, 0);
    if (tok[0] == '\0' || (endp && *endp != '\0') || errno == ERANGE) {
        return CFGPACK_ERR_PARSE;
    }

    switch (type) {
        case CFGPACK_TYPE_I8:  min_val = -128; max_val = 127; break;
        case CFGPACK_TYPE_I16: min_val = -32768; max_val = 32767; break;
        case CFGPACK_TYPE_I32: min_val = -2147483648LL; max_val = 2147483647LL; break;
        case CFGPACK_TYPE_I64: min_val = INT64_MIN; max_val = INT64_MAX; break;
        default: return CFGPACK_ERR_INVALID_TYPE;
    }

    if (val < min_val || val > max_val) return CFGPACK_ERR_BOUNDS;

    out->type = type;
    out->v.i64 = val;
    return CFGPACK_OK;
}

/**
 * @brief Parse a float default value.
 * @param tok   Token string.
 * @param out   Output value structure.
 * @param type  Expected float type (f32 or f64).
 * @return CFGPACK_OK on success, error code on failure.
 */
static cfgpack_err_t parse_float(const char *tok, cfgpack_value_t *out, cfgpack_type_t type) {
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
 * @param line       Mutable line buffer positioned after type token.
 * @param out_tok    Output buffer for extracted token.
 * @param tok_size   Size of output buffer.
 * @return Pointer to extracted token, or NULL on error.
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
 * @param tok       Default value token.
 * @param type      Expected type.
 * @param out       Output value structure.
 * @param has_def   Output flag: 1 if default exists, 0 if NIL.
 * @return CFGPACK_OK on success, error code on failure.
 */
static cfgpack_err_t parse_default(const char *tok, cfgpack_type_t type, cfgpack_value_t *out, uint8_t *has_def) {
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
        case CFGPACK_TYPE_U64:
            return parse_uint(tok, out, type);

        case CFGPACK_TYPE_I8:
        case CFGPACK_TYPE_I16:
        case CFGPACK_TYPE_I32:
        case CFGPACK_TYPE_I64:
            return parse_int(tok, out, type);

        case CFGPACK_TYPE_F32:
        case CFGPACK_TYPE_F64:
            return parse_float(tok, out, type);

        case CFGPACK_TYPE_STR:
        case CFGPACK_TYPE_FSTR:
            return parse_quoted_string(tok, out, type, NULL);
    }

    return CFGPACK_ERR_INVALID_TYPE;
}

cfgpack_err_t cfgpack_parse_schema(const char *path, cfgpack_schema_t *out_schema, cfgpack_entry_t *entries, size_t max_entries, cfgpack_value_t *defaults, cfgpack_parse_error_t *err) {
    FILE *f = fopen(path, "r");
    char line[MAX_LINE_LEN];
    size_t line_no = 0;
    int header_read = 0;
    size_t count = 0;

    if (!f) {
        set_err(err, 0, "unable to open file");
        return CFGPACK_ERR_PARSE;
    }

    while (fgets(line, sizeof(line), f)) {
        char *slots[4];
        tokens_t tok;
        size_t stop_offset = 0;

        line_no++;
        if (is_blank_or_comment(line)) continue;

        if (tokens_create(&tok, 4, slots) != 0) {
            fclose(f);
            return CFGPACK_ERR_PARSE;
        }

        if (!header_read) {
            tokens_find(&tok, line, " \t\r\n", 2, NULL);
            if (tok.used != 2) {
                set_err(err, line_no, "invalid header");
                fclose(f);
                tokens_destroy(&tok);
                return CFGPACK_ERR_PARSE;
            }
            if (strlen(tok.index[0]) >= sizeof(out_schema->map_name)) {
                set_err(err, line_no, "map name too long");
                fclose(f);
                tokens_destroy(&tok);
                return CFGPACK_ERR_BOUNDS;
            }
            char *endp = NULL;
            unsigned long ver = strtoul(tok.index[1], &endp, 10);
            if (tok.index[1][0] == '\0' || (endp && *endp != '\0')) {
                set_err(err, line_no, "invalid header");
                fclose(f);
                tokens_destroy(&tok);
                return CFGPACK_ERR_PARSE;
            }
            if (ver > 0xfffffffful) {
                set_err(err, line_no, "version out of range");
                fclose(f);
                tokens_destroy(&tok);
                return CFGPACK_ERR_BOUNDS;
            }
            snprintf(out_schema->map_name, sizeof(out_schema->map_name), "%s", tok.index[0]);
            out_schema->version = (uint32_t)ver;
            tokens_destroy(&tok);
            header_read = 1;
            continue;
        }

        /* Parse first 3 tokens: index, name, type */
        tokens_find(&tok, line, " \t\r\n", 3, &stop_offset);

        if (tok.used < 3) {
            set_err(err, line_no, "invalid entry");
            fclose(f);
            tokens_destroy(&tok);
            return CFGPACK_ERR_PARSE;
        }

        if (count >= max_entries) {
            set_err(err, line_no, "too many entries");
            fclose(f);
            tokens_destroy(&tok);
            return CFGPACK_ERR_BOUNDS;
        }

        unsigned long idx_ul = strtoul(tok.index[0], NULL, 10);
        if (idx_ul > 65535ul) {
            set_err(err, line_no, "index out of range");
            fclose(f);
            tokens_destroy(&tok);
            return CFGPACK_ERR_BOUNDS;
        }
        cfgpack_type_t type;
        cfgpack_err_t trc = parse_type(tok.index[2], &type);
        if (trc != CFGPACK_OK) {
            set_err(err, line_no, "invalid type");
            fclose(f);
            tokens_destroy(&tok);
            return trc;
        }
        if (name_too_long(tok.index[1])) {
            set_err(err, line_no, "name too long");
            fclose(f);
            tokens_destroy(&tok);
            return CFGPACK_ERR_BOUNDS;
        }
        if (has_duplicate(entries, count, (uint16_t)idx_ul, tok.index[1])) {
            set_err(err, line_no, "duplicate");
            fclose(f);
            tokens_destroy(&tok);
            return CFGPACK_ERR_DUPLICATE;
        }

        /* Extract default value from remainder of line */
        char default_tok[MAX_LINE_LEN];
        /* Find where the original line's type token ended */
        const char *remainder = line + stop_offset;
        const char *def_str = extract_default_token(remainder, default_tok, sizeof(default_tok));
        if (!def_str || def_str[0] == '\0') {
            set_err(err, line_no, "missing default value");
            fclose(f);
            tokens_destroy(&tok);
            return CFGPACK_ERR_PARSE;
        }

        uint8_t has_def = 0;
        cfgpack_err_t drc = parse_default(def_str, type, &defaults[count], &has_def);
        if (drc != CFGPACK_OK) {
            set_err(err, line_no, "invalid default value");
            fclose(f);
            tokens_destroy(&tok);
            return drc;
        }

        entries[count].index = (uint16_t)idx_ul;
        snprintf(entries[count].name, sizeof(entries[count].name), "%s", tok.index[1]);
        entries[count].type = type;
        entries[count].has_default = has_def;
        count++;
        tokens_destroy(&tok);
    }

    fclose(f);

    if (!header_read) {
        set_err(err, 0, "missing header");
        return CFGPACK_ERR_PARSE;
    }

    sort_entries(entries, defaults, count);
    out_schema->entries = entries;
    out_schema->entry_count = count;
    return CFGPACK_OK;
}

void cfgpack_schema_free(cfgpack_schema_t *schema) {
    (void)schema; /* no-op: caller owns buffers */
}

/**
 * @brief Convert a type enum to its string representation.
 * @param type  Type enum value.
 * @return String representation of the type.
 */
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

/**
 * @brief Format a default value as a string for markdown output.
 * @param entry    Schema entry (for type and has_default flag).
 * @param val      Default value.
 * @param buf      Output buffer.
 * @param buf_size Size of output buffer.
 */
static void format_default(const cfgpack_entry_t *entry, const cfgpack_value_t *val, char *buf, size_t buf_size) {
    if (!entry->has_default) {
        snprintf(buf, buf_size, "NIL");
        return;
    }

    switch (entry->type) {
        case CFGPACK_TYPE_U8:
        case CFGPACK_TYPE_U16:
        case CFGPACK_TYPE_U32:
        case CFGPACK_TYPE_U64:
            snprintf(buf, buf_size, "%llu", (unsigned long long)val->v.u64);
            break;
        case CFGPACK_TYPE_I8:
        case CFGPACK_TYPE_I16:
        case CFGPACK_TYPE_I32:
        case CFGPACK_TYPE_I64:
            snprintf(buf, buf_size, "%lld", (long long)val->v.i64);
            break;
        case CFGPACK_TYPE_F32:
            snprintf(buf, buf_size, "%g", (double)val->v.f32);
            break;
        case CFGPACK_TYPE_F64:
            snprintf(buf, buf_size, "%g", val->v.f64);
            break;
        case CFGPACK_TYPE_STR:
            snprintf(buf, buf_size, "\"%s\"", val->v.str.data);
            break;
        case CFGPACK_TYPE_FSTR:
            snprintf(buf, buf_size, "\"%s\"", val->v.fstr.data);
            break;
        default:
            snprintf(buf, buf_size, "?");
            break;
    }
}

cfgpack_err_t cfgpack_schema_write_markdown(const cfgpack_schema_t *schema, const cfgpack_value_t *defaults, const char *out_path, cfgpack_parse_error_t *err) {
    FILE *f = fopen(out_path, "w");
    if (!f) {
        set_err(err, 0, "unable to write file");
        return CFGPACK_ERR_IO;
    }
    fprintf(f, "# %s\n", schema->map_name);
    fprintf(f, "Version: %u\n\n", schema->version);
    fprintf(f, "| Index | Name | Type | Default |\n| --- | --- | --- | --- |\n");
    for (size_t i = 0; i < schema->entry_count; ++i) {
        const cfgpack_entry_t *e = &schema->entries[i];
        char default_str[128];
        format_default(e, &defaults[i], default_str, sizeof(default_str));
        fprintf(f, "| %u | %s | %s | %s |\n", (unsigned)e->index, e->name, type_to_str(e->type), default_str);
    }
    fclose(f);
    (void)err;
    return CFGPACK_OK;
}
