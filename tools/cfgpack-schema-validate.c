/**
 * @file cfgpack-schema-validate.c
 * @brief CLI tool for validating cfgpack schema files.
 *
 * Usage:
 *   cfgpack-schema-validate [--lz4|--heatshrink] [--format map|json|msgpack] <input>
 *
 * The input file format is auto-detected by extension:
 *   - Files ending in ".json" are parsed as JSON schemas.
 *   - Files ending in ".msgpack" or ".bin" are parsed as MessagePack binary.
 *   - All other files are parsed as .map schemas.
 *
 * The --format flag overrides extension-based detection.
 *
 * When --lz4 or --heatshrink is specified, the file is decompressed first.
 * Compressed schemas are always MessagePack binary underneath.
 *
 * Exit codes:
 *   0 - Valid schema
 *   1 - Usage error
 *   2 - File I/O error
 *   3 - Validation error
 */

#include "cfgpack/cfgpack.h"
#include "cfgpack/schema.h"
#include "cfgpack/value.h"

#include "heatshrink_decoder.h"
#include "lz4.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT_SIZE (64 * 1024)
#define MAX_SCRATCH_SIZE (64 * 1024)
#define MAX_ENTRIES 256
#define MAX_STR_OFFSETS 256

enum { FMT_MAP = 0, FMT_JSON = 1, FMT_MSGPACK = 2 };

enum { COMPRESS_NONE = 0, COMPRESS_LZ4 = 1, COMPRESS_HEATSHRINK = 2 };

static uint8_t input_buf[MAX_INPUT_SIZE];
static uint8_t scratch_buf[MAX_SCRATCH_SIZE];

static cfgpack_entry_t entries[MAX_ENTRIES];
static cfgpack_value_t values[MAX_ENTRIES];
static char str_pool[MAX_STR_OFFSETS * (CFGPACK_STR_MAX + 1)];
static uint16_t str_offsets[MAX_STR_OFFSETS];

/* ─────────────────────────────────────────────────────────────────────────────
 * Helpers
 * ───────────────────────────────────────────────────────────────────────────── */

static void print_usage(const char *prog) {
    fprintf(stderr,
            "Usage: %s [--lz4|--heatshrink] [--format map|json|msgpack]"
            " <input>\n",
            prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Validates a cfgpack schema file.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --lz4          Decompress input with LZ4\n");
    fprintf(stderr, "  --heatshrink   Decompress input with heatshrink\n");
    fprintf(stderr, "  --format FMT   Force format: map, json, or msgpack\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Format auto-detection (by extension):\n");
    fprintf(stderr, "  .json          JSON schema\n");
    fprintf(stderr, "  .msgpack .bin  MessagePack binary\n");
    fprintf(stderr, "  Other          .map plain text schema\n");
}

static int has_suffix(const char *str, const char *suffix) {
    size_t str_len = strlen(str);
    size_t suf_len = strlen(suffix);
    if (suf_len > str_len) {
        return (0);
    }
    return (strcmp(str + str_len - suf_len, suffix) == 0);
}

static size_t read_file(const char *path, uint8_t *buf, size_t cap) {
    size_t n;
    FILE *f;

    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open file: %s\n", path);
        return (0);
    }

    n = fread(buf, 1, cap, f);
    if (n == 0 && ferror(f)) {
        fprintf(stderr, "Error reading file: %s\n", path);
        fclose(f);
        return (0);
    }
    if (!feof(f)) {
        fprintf(stderr, "File too large (max %d bytes): %s\n", MAX_INPUT_SIZE,
                path);
        fclose(f);
        return (0);
    }

    fclose(f);
    return (n);
}

static int detect_format(const char *path) {
    if (has_suffix(path, ".json")) {
        return (FMT_JSON);
    }
    if (has_suffix(path, ".msgpack") || has_suffix(path, ".bin")) {
        return (FMT_MSGPACK);
    }
    return (FMT_MAP);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Decompression
 * ───────────────────────────────────────────────────────────────────────────── */

static cfgpack_err_t decompress_lz4(const uint8_t *in,
                                    size_t in_len,
                                    uint8_t *out,
                                    size_t out_cap,
                                    size_t *out_len) {
    uint32_t orig_size;
    int result;

    if (in_len < 4) {
        fprintf(stderr, "LZ4 input too short (missing size header)\n");
        return (CFGPACK_ERR_DECODE);
    }

    /* Read 4-byte little-endian original size header */
    orig_size = (uint32_t)in[0] | ((uint32_t)in[1] << 8) |
                ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);

    if ((size_t)orig_size > out_cap) {
        fprintf(stderr, "Decompressed size %u exceeds buffer capacity\n",
                orig_size);
        return (CFGPACK_ERR_BOUNDS);
    }

    result = LZ4_decompress_safe((const char *)(in + 4), (char *)out,
                                 (int)(in_len - 4), (int)orig_size);
    if (result < 0 || (size_t)result != (size_t)orig_size) {
        fprintf(stderr, "LZ4 decompression failed\n");
        return (CFGPACK_ERR_DECODE);
    }

    *out_len = (size_t)orig_size;
    return (CFGPACK_OK);
}

static heatshrink_decoder hs_decoder;

static cfgpack_err_t decompress_heatshrink(const uint8_t *in,
                                           size_t in_len,
                                           uint8_t *out,
                                           size_t out_cap,
                                           size_t *out_len) {
    size_t output_produced = 0;
    size_t input_consumed = 0;
    HSD_finish_res finish_res;
    size_t total_output = 0;
    HSD_sink_res sink_res;
    HSD_poll_res poll_res;

    heatshrink_decoder_reset(&hs_decoder);

    while (input_consumed < in_len) {
        sink_res = heatshrink_decoder_sink(&hs_decoder,
                                           (uint8_t *)(in + input_consumed),
                                           in_len - input_consumed,
                                           &output_produced);
        if (sink_res < 0) {
            fprintf(stderr, "Heatshrink sink failed\n");
            return (CFGPACK_ERR_DECODE);
        }
        input_consumed += output_produced;

        do {
            poll_res = heatshrink_decoder_poll(&hs_decoder, out + total_output,
                                               out_cap - total_output,
                                               &output_produced);
            if (poll_res < 0) {
                fprintf(stderr, "Heatshrink poll failed\n");
                return (CFGPACK_ERR_DECODE);
            }
            total_output += output_produced;

            if (total_output > out_cap) {
                fprintf(stderr, "Decompressed data exceeds buffer capacity\n");
                return (CFGPACK_ERR_BOUNDS);
            }
        } while (poll_res == HSDR_POLL_MORE);
    }

    finish_res = heatshrink_decoder_finish(&hs_decoder);
    if (finish_res < 0) {
        fprintf(stderr, "Heatshrink finish failed\n");
        return (CFGPACK_ERR_DECODE);
    }

    while (finish_res == HSDR_FINISH_MORE) {
        poll_res = heatshrink_decoder_poll(&hs_decoder, out + total_output,
                                           out_cap - total_output,
                                           &output_produced);
        if (poll_res < 0) {
            fprintf(stderr, "Heatshrink poll failed\n");
            return (CFGPACK_ERR_DECODE);
        }
        total_output += output_produced;

        if (total_output > out_cap) {
            fprintf(stderr, "Decompressed data exceeds buffer capacity\n");
            return (CFGPACK_ERR_BOUNDS);
        }

        finish_res = heatshrink_decoder_finish(&hs_decoder);
        if (finish_res < 0) {
            fprintf(stderr, "Heatshrink finish failed\n");
            return (CFGPACK_ERR_DECODE);
        }
    }

    *out_len = total_output;
    return (CFGPACK_OK);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Validation
 * ───────────────────────────────────────────────────────────────────────────── */

static void print_type_summary(const cfgpack_schema_t *schema) {
    size_t counts[CFGPACK_TYPE_FSTR + 1] = {0};
    const char *names[] = {"u8",  "u16", "u32", "u64", "i8",  "i16",
                           "i32", "i64", "f32", "f64", "str", "fstr"};
    int first = 1;
    size_t i;

    for (i = 0; i < schema->entry_count; i++) {
        counts[schema->entries[i].type]++;
    }

    printf("  Types:");
    for (i = 0; i <= CFGPACK_TYPE_FSTR; i++) {
        if (counts[i] > 0) {
            printf("%s %zu %s", first ? "" : ",", counts[i], names[i]);
            first = 0;
        }
    }
    printf("\n");
}

static int validate_schema(const uint8_t *data, size_t data_len, int format) {
    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t perr;
    cfgpack_parse_opts_t opts;
    cfgpack_schema_t schema;
    cfgpack_err_t rc;

    memset(&perr, 0, sizeof(perr));

    /* Phase 1: measure (validates structure) */
    switch (format) {
    case FMT_MAP:
        rc = cfgpack_schema_measure((const char *)data, data_len, &m, &perr);
        break;
    case FMT_JSON:
        rc = cfgpack_schema_measure_json((const char *)data, data_len, &m,
                                         &perr);
        break;
    case FMT_MSGPACK:
        rc = cfgpack_schema_measure_msgpack(data, data_len, &m, &perr);
        break;
    default: fprintf(stderr, "Internal error: unknown format\n"); return (3);
    }

    if (rc != CFGPACK_OK) {
        if (perr.line > 0) {
            fprintf(stderr, "Invalid: line %zu: %s\n", perr.line, perr.message);
        } else {
            fprintf(stderr, "Invalid: %s\n", perr.message);
        }
        return (3);
    }

    if (m.entry_count > MAX_ENTRIES) {
        fprintf(stderr, "Invalid: too many entries: %zu (max %d)\n",
                m.entry_count, MAX_ENTRIES);
        return (3);
    }
    if (m.str_count + m.fstr_count > MAX_STR_OFFSETS) {
        fprintf(stderr, "Invalid: too many string entries: %zu (max %d)\n",
                m.str_count + m.fstr_count, MAX_STR_OFFSETS);
        return (3);
    }

    /* Phase 2: full parse (catches duplicates) */
    memset(&opts, 0, sizeof(opts));
    opts.out_schema = &schema;
    opts.entries = entries;
    opts.max_entries = m.entry_count;
    opts.values = values;
    opts.str_pool = str_pool;
    opts.str_pool_cap = m.str_pool_size;
    opts.str_offsets = str_offsets;
    opts.str_offsets_count = m.str_count + m.fstr_count;
    opts.err = &perr;

    switch (format) {
    case FMT_MAP:
        rc = cfgpack_parse_schema((const char *)data, data_len, &opts);
        break;
    case FMT_JSON:
        rc = cfgpack_schema_parse_json((const char *)data, data_len, &opts);
        break;
    case FMT_MSGPACK:
        rc = cfgpack_schema_parse_msgpack(data, data_len, &opts);
        break;
    default: return (3);
    }

    if (rc != CFGPACK_OK) {
        if (perr.line > 0) {
            fprintf(stderr, "Invalid: line %zu: %s\n", perr.line, perr.message);
        } else {
            fprintf(stderr, "Invalid: %s\n", perr.message);
        }
        return (3);
    }

    /* Phase 3: print success summary */
    printf("Valid: \"%s\" v%u (%zu entries)\n", schema.map_name, schema.version,
           schema.entry_count);
    print_type_summary(&schema);

    return (0);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Main
 * ───────────────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[]) {
    const char *input_path = NULL;
    const char *fmt_str = NULL;
    int compression = COMPRESS_NONE;
    int format = -1;
    const uint8_t *data;
    size_t data_len;
    size_t file_len;
    cfgpack_err_t rc;
    int i;

    /* Parse arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--lz4") == 0) {
            compression = COMPRESS_LZ4;
        } else if (strcmp(argv[i], "--heatshrink") == 0) {
            compression = COMPRESS_HEATSHRINK;
        } else if (strcmp(argv[i], "--format") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Error: --format requires an argument\n");
                return (1);
            }
            fmt_str = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 ||
                   strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return (0);
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return (1);
        } else {
            if (input_path) {
                fprintf(stderr, "Error: multiple input files specified\n");
                return (1);
            }
            input_path = argv[i];
        }
    }

    if (!input_path) {
        print_usage(argv[0]);
        return (1);
    }

    /* Resolve format */
    if (fmt_str) {
        if (strcmp(fmt_str, "map") == 0) {
            format = FMT_MAP;
        } else if (strcmp(fmt_str, "json") == 0) {
            format = FMT_JSON;
        } else if (strcmp(fmt_str, "msgpack") == 0) {
            format = FMT_MSGPACK;
        } else {
            fprintf(stderr,
                    "Error: unknown format \"%s\""
                    " (expected: map, json, msgpack)\n",
                    fmt_str);
            return (1);
        }
    }

    /* Read input file */
    file_len = read_file(input_path, input_buf, sizeof(input_buf));
    if (file_len == 0) {
        return (2);
    }

    /* Decompress if needed */
    data = input_buf;
    data_len = file_len;

    if (compression == COMPRESS_LZ4) {
        size_t decompressed_len = 0;
        rc = decompress_lz4(input_buf, file_len, scratch_buf,
                            sizeof(scratch_buf), &decompressed_len);
        if (rc != CFGPACK_OK) {
            return (2);
        }
        data = scratch_buf;
        data_len = decompressed_len;
        if (format < 0) {
            format = FMT_MSGPACK;
        }
    } else if (compression == COMPRESS_HEATSHRINK) {
        size_t decompressed_len = 0;
        rc = decompress_heatshrink(input_buf, file_len, scratch_buf,
                                   sizeof(scratch_buf), &decompressed_len);
        if (rc != CFGPACK_OK) {
            return (2);
        }
        data = scratch_buf;
        data_len = decompressed_len;
        if (format < 0) {
            format = FMT_MSGPACK;
        }
    }

    /* Auto-detect format from extension if not already set */
    if (format < 0) {
        format = detect_format(input_path);
    }

    return (validate_schema(data, data_len, format));
}
