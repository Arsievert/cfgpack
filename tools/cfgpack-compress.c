/**
 * @file cfgpack-compress.c
 * @brief CLI tool for compressing files with LZ4 or heatshrink.
 *
 * Usage:
 *   cfgpack-compress <algorithm> <input> <output>
 *
 * Algorithms:
 *   lz4        - LZ4 block compression
 *   heatshrink - Heatshrink compression (window=8, lookahead=4)
 *
 * The output file contains raw compressed data. For LZ4, the original
 * (decompressed) size must be stored separately as it's required for
 * decompression.
 *
 * Exit codes:
 *   0 - Success
 *   1 - Usage error
 *   2 - File I/O error
 *   3 - Compression error
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "lz4.h"
#include "heatshrink_encoder.h"

#define MAX_INPUT_SIZE (64 * 1024)  /* 64 KB max input */
#define MAX_OUTPUT_SIZE (64 * 1024) /* 64 KB max output */

static uint8_t input_buf[MAX_INPUT_SIZE];
static uint8_t output_buf[MAX_OUTPUT_SIZE];

/* Static heatshrink encoder (uses config from heatshrink_config.h) */
static heatshrink_encoder hs_encoder;

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s <algorithm> <input> <output>\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Algorithms:\n");
    fprintf(stderr, "  lz4        - LZ4 block compression\n");
    fprintf(stderr, "  heatshrink - Heatshrink compression (window=8, lookahead=4)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Output format:\n");
    fprintf(stderr, "  Raw compressed data. For LZ4, store original size separately.\n");
}

static int compress_lz4(const uint8_t *input,
                        size_t input_len,
                        uint8_t *output,
                        size_t output_cap,
                        size_t *output_len) {
    int compressed_size = LZ4_compress_default((const char *)input, (char *)output, (int)input_len, (int)output_cap);

    if (compressed_size <= 0) {
        fprintf(stderr, "LZ4 compression failed\n");
        return -1;
    }

    *output_len = (size_t)compressed_size;
    return 0;
}

static int compress_heatshrink(const uint8_t *input,
                               size_t input_len,
                               uint8_t *output,
                               size_t output_cap,
                               size_t *output_len) {
    size_t input_consumed = 0;
    size_t total_output = 0;
    size_t sink_count, poll_count;
    HSE_sink_res sink_res;
    HSE_poll_res poll_res;
    HSE_finish_res finish_res;

    heatshrink_encoder_reset(&hs_encoder);

    /* Feed input and poll for output */
    while (input_consumed < input_len) {
        sink_res = heatshrink_encoder_sink(&hs_encoder,
                                           (uint8_t *)(input + input_consumed),
                                           input_len - input_consumed,
                                           &sink_count);
        if (sink_res < 0) {
            fprintf(stderr, "Heatshrink sink error: %d\n", sink_res);
            return -1;
        }
        input_consumed += sink_count;

        /* Poll for compressed output */
        do {
            poll_res =
                heatshrink_encoder_poll(&hs_encoder, output + total_output, output_cap - total_output, &poll_count);
            if (poll_res < 0) {
                fprintf(stderr, "Heatshrink poll error: %d\n", poll_res);
                return -1;
            }
            total_output += poll_count;

            if (total_output > output_cap) {
                fprintf(stderr, "Output buffer overflow\n");
                return -1;
            }
        } while (poll_res == HSER_POLL_MORE);
    }

    /* Finish encoding */
    do {
        finish_res = heatshrink_encoder_finish(&hs_encoder);
        if (finish_res < 0) {
            fprintf(stderr, "Heatshrink finish error: %d\n", finish_res);
            return -1;
        }

        /* Poll remaining output */
        do {
            poll_res =
                heatshrink_encoder_poll(&hs_encoder, output + total_output, output_cap - total_output, &poll_count);
            if (poll_res < 0) {
                fprintf(stderr, "Heatshrink poll error: %d\n", poll_res);
                return -1;
            }
            total_output += poll_count;

            if (total_output > output_cap) {
                fprintf(stderr, "Output buffer overflow\n");
                return -1;
            }
        } while (poll_res == HSER_POLL_MORE);
    } while (finish_res == HSER_FINISH_MORE);

    *output_len = total_output;
    return 0;
}

int main(int argc, char *argv[]) {
    FILE *fin = NULL;
    FILE *fout = NULL;
    size_t input_len, output_len;
    int ret = 0;
    int is_lz4;

    if (argc != 4) {
        print_usage(argv[0]);
        return 1;
    }

    const char *algorithm = argv[1];
    const char *input_path = argv[2];
    const char *output_path = argv[3];

    /* Validate algorithm */
    if (strcmp(algorithm, "lz4") == 0) {
        is_lz4 = 1;
    } else if (strcmp(algorithm, "heatshrink") == 0) {
        is_lz4 = 0;
    } else {
        fprintf(stderr, "Unknown algorithm: %s\n", algorithm);
        print_usage(argv[0]);
        return 1;
    }

    /* Read input file */
    fin = fopen(input_path, "rb");
    if (!fin) {
        fprintf(stderr, "Cannot open input file: %s\n", input_path);
        return 2;
    }

    input_len = fread(input_buf, 1, MAX_INPUT_SIZE, fin);
    if (ferror(fin)) {
        fprintf(stderr, "Error reading input file\n");
        fclose(fin);
        return 2;
    }
    if (!feof(fin)) {
        fprintf(stderr, "Input file too large (max %d bytes)\n", MAX_INPUT_SIZE);
        fclose(fin);
        return 2;
    }
    fclose(fin);
    fin = NULL;

    /* Compress */
    if (is_lz4) {
        ret = compress_lz4(input_buf, input_len, output_buf, MAX_OUTPUT_SIZE, &output_len);
    } else {
        ret = compress_heatshrink(input_buf, input_len, output_buf, MAX_OUTPUT_SIZE, &output_len);
    }

    if (ret != 0) {
        return 3;
    }

    /* Write output file */
    fout = fopen(output_path, "wb");
    if (!fout) {
        fprintf(stderr, "Cannot open output file: %s\n", output_path);
        return 2;
    }

    if (fwrite(output_buf, 1, output_len, fout) != output_len) {
        fprintf(stderr, "Error writing output file\n");
        fclose(fout);
        return 2;
    }
    fclose(fout);

    /* Print stats */
    printf("%s: %zu -> %zu bytes (%.1f%%)\n",
           algorithm,
           input_len,
           output_len,
           input_len > 0 ? (100.0 * output_len / input_len) : 0.0);
    printf("Original size: %zu (needed for LZ4 decompression)\n", input_len);

    return 0;
}
