#include "cfgpack/schema.h"
#include "cfgpack/error.h"
#include "cfgpack/value.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
 * @brief Sort entries by index using insertion sort.
 * @param entries  Array of entries to sort in-place.
 * @param n        Number of entries.
 */
static void sort_entries(cfgpack_entry_t *entries, size_t n) {
    for (size_t i = 1; i < n; ++i) {
        cfgpack_entry_t key = entries[i];
        size_t j = i;
        while (j > 0 && entries[j - 1].index > key.index) {
            entries[j] = entries[j - 1];
            --j;
        }
        entries[j] = key;
    }
}

cfgpack_err_t cfgpack_parse_schema(const char *path, cfgpack_schema_t *out_schema, cfgpack_entry_t *entries, size_t max_entries, cfgpack_parse_error_t *err) {
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
        char *slots[3];
        tokens_t tok;

        line_no++;
        if (is_blank_or_comment(line)) continue;

        if (tokens_create(&tok, 3, slots) != 0) {
            fclose(f);
            return CFGPACK_ERR_PARSE;
        }
        tokens_find(&tok, line, " \t\r\n", 3, NULL);

        if (!header_read) {
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

        if (tok.used != 3) {
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

        entries[count].index = (uint16_t)idx_ul;
        snprintf(entries[count].name, sizeof(entries[count].name), "%s", tok.index[1]);
        entries[count].type = type;
        count++;
        tokens_destroy(&tok);
    }

    fclose(f);

    if (!header_read) {
        set_err(err, 0, "missing header");
        return CFGPACK_ERR_PARSE;
    }

    sort_entries(entries, count);
    out_schema->entries = entries;
    out_schema->entry_count = count;
    return CFGPACK_OK;
}

void cfgpack_schema_free(cfgpack_schema_t *schema) {
    (void)schema; /* no-op: caller owns buffers */
}

cfgpack_err_t cfgpack_schema_write_markdown(const cfgpack_schema_t *schema, const char *out_path, cfgpack_parse_error_t *err) {
    FILE *f = fopen(out_path, "w");
    if (!f) {
        set_err(err, 0, "unable to write file");
        return CFGPACK_ERR_IO;
    }
    fprintf(f, "# %s\n", schema->map_name);
    fprintf(f, "Version: %u\n\n", schema->version);
    fprintf(f, "| Index | Name | Type |\n| --- | --- | --- |\n");
    for (size_t i = 0; i < schema->entry_count; ++i) {
        const cfgpack_entry_t *e = &schema->entries[i];
        const char *type_str = "";
        switch (e->type) {
            case CFGPACK_TYPE_U8: type_str = "u8"; break;
            case CFGPACK_TYPE_U16: type_str = "u16"; break;
            case CFGPACK_TYPE_U32: type_str = "u32"; break;
            case CFGPACK_TYPE_U64: type_str = "u64"; break;
            case CFGPACK_TYPE_I8: type_str = "i8"; break;
            case CFGPACK_TYPE_I16: type_str = "i16"; break;
            case CFGPACK_TYPE_I32: type_str = "i32"; break;
            case CFGPACK_TYPE_I64: type_str = "i64"; break;
            case CFGPACK_TYPE_F32: type_str = "f32"; break;
            case CFGPACK_TYPE_F64: type_str = "f64"; break;
            case CFGPACK_TYPE_STR: type_str = "str"; break;
            case CFGPACK_TYPE_FSTR: type_str = "fstr"; break;
        }
        fprintf(f, "| %u | %s | %s |\n", (unsigned)e->index, e->name, type_str);
    }
    fclose(f);
    (void)err;
    return CFGPACK_OK;
}
