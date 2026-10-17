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
 * Output format:
 *   LZ4:        4-byte little-endian original size + raw compressed data
 *   Heatshrink: raw compressed data only
 *
 * Exit codes:
 *   0 - Success
 *   1 - Usage error
 *   2 - File I/O error
 *   3 - Compression error
 */

#include "heatshrink_encoder.h"
#include "lz4.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT_SIZE (64 * 1024) /* 64 KB max input */
/* Worst case for either codec on incompressible input: LZ4 needs
 * LZ4_COMPRESSBOUND(n) = n + n/255 + 16; heatshrink's literal path costs
 * 9 bits per byte, so n + n/8.  Round up generously. */
#define MAX_OUTPUT_SIZE (MAX_INPUT_SIZE + MAX_INPUT_SIZE / 8 + 64)

static uint8_t input_buf[MAX_INPUT_SIZE];
static uint8_t output_buf[MAX_OUTPUT_SIZE];

/* Static heatshrink encoder (uses config from heatshrink_config.h) */
static heatshrink_encoder hs_encoder;

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s <algorithm> <input> <output>\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Algorithms:\n");
    fprintf(stderr, "  lz4        - LZ4 block compression\n");
    fprintf(stderr,
            "  heatshrink - Heatshrink compression (window=8, lookahead=4)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Output format:\n");
    fprintf(stderr,
            "  LZ4:        4-byte LE original size + compressed data\n");
    fprintf(stderr, "  Heatshrink: raw compressed data only\n");
}

static int compress_lz4(const uint8_t *input,
                        size_t input_len,
                        uint8_t *output,
                        size_t output_cap,
                        size_t *output_len) {
    int compressed_size = LZ4_compress_default((const char *)input,
                                               (char *)output, (int)input_len,
                                               (int)output_cap);

    if (compressed_size <= 0) {
        fprintf(stderr, "LZ4 compression failed\n");
        return (-1);
    }

    *output_len = (size_t)compressed_size;
    return (0);
}

/**
 * @brief Drain pending encoder output.
 *
 * heatshrink_encoder_poll() rejects a zero-length output buffer with
 * HSER_POLL_ERROR_MISUSE, so a full buffer is reported as an overflow here
 * before polling rather than as a confusing poll error.
 */
static int drain_encoder(uint8_t *output, size_t output_cap, size_t *total) {
    HSE_poll_res poll_res;
    size_t produced;

    do {
        if (*total >= output_cap) {
            fprintf(stderr, "Output buffer overflow\n");
            return (-1);
        }
        poll_res = heatshrink_encoder_poll(&hs_encoder, output + *total,
                                           output_cap - *total, &produced);
        if (poll_res < 0) {
            fprintf(stderr, "Heatshrink poll error: %d\n", poll_res);
            return (-1);
        }
        *total += produced;
    } while (poll_res == HSER_POLL_MORE);
    return (0);
}

static int compress_heatshrink(const uint8_t *input,
                               size_t input_len,
                               uint8_t *output,
                               size_t output_cap,
                               size_t *output_len) {
    HSE_finish_res finish_res;
    size_t input_consumed = 0;
    size_t total_output = 0;
    HSE_sink_res sink_res;
    size_t sink_count;

    heatshrink_encoder_reset(&hs_encoder);

    /* Feed input and drain output after each chunk */
    while (input_consumed < input_len) {
        sink_res = heatshrink_encoder_sink(&hs_encoder,
                                           (uint8_t *)(input + input_consumed),
                                           input_len - input_consumed,
                                           &sink_count);
        if (sink_res < 0 || sink_count == 0) {
            fprintf(stderr, "Heatshrink sink error: %d\n", sink_res);
            return (-1);
        }
        input_consumed += sink_count;
        if (drain_encoder(output, output_cap, &total_output) != 0) {
            return (-1);
        }
    }

    /* Finish encoding */
    do {
        finish_res = heatshrink_encoder_finish(&hs_encoder);
        if (finish_res < 0) {
            fprintf(stderr, "Heatshrink finish error: %d\n", finish_res);
            return (-1);
        }
        if (finish_res == HSER_FINISH_MORE &&
            drain_encoder(output, output_cap, &total_output) != 0) {
            return (-1);
        }
    } while (finish_res == HSER_FINISH_MORE);

    *output_len = total_output;
    return (0);
}

int main(int argc, char *argv[]) {
    const char *output_path;
    const char *input_path;
    const char *algorithm;
    size_t output_len = 0;
    size_t file_size;
    size_t input_len;
    FILE *fin = NULL;
    FILE *fout = NULL;
    int ret = 0;
    int is_lz4;

    if (argc != 4) {
        print_usage(argv[0]);
        return (1);
    }

    algorithm = argv[1];
    input_path = argv[2];
    output_path = argv[3];

    /* Validate algorithm */
    if (strcmp(algorithm, "lz4") == 0) {
        is_lz4 = 1;
    } else if (strcmp(algorithm, "heatshrink") == 0) {
        is_lz4 = 0;
    } else {
        fprintf(stderr, "Unknown algorithm: %s\n", algorithm);
        print_usage(argv[0]);
        return (1);
    }

    /* Read input file */
    fin = fopen(input_path, "rb");
    if (!fin) {
        fprintf(stderr, "Cannot open input file: %s\n", input_path);
        return (2);
    }

    input_len = fread(input_buf, 1, MAX_INPUT_SIZE, fin);
    if (ferror(fin)) {
        fprintf(stderr, "Error reading input file\n");
        fclose(fin);
        return (2);
    }
    /* feof() is only set by a read past the end, so probe one more byte:
     * a file of exactly MAX_INPUT_SIZE bytes is fine. */
    if (input_len == MAX_INPUT_SIZE && fgetc(fin) != EOF) {
        fprintf(stderr, "Input file too large (max %d bytes)\n",
                MAX_INPUT_SIZE);
        fclose(fin);
        return (2);
    }
    fclose(fin);
    fin = NULL;

    /* Compress */
    if (is_lz4) {
        ret = compress_lz4(input_buf, input_len, output_buf, MAX_OUTPUT_SIZE,
                           &output_len);
    } else {
        ret = compress_heatshrink(input_buf, input_len, output_buf,
                                  MAX_OUTPUT_SIZE, &output_len);
    }

    if (ret != 0) {
        return (3);
    }

    /* Write output file */
    fout = fopen(output_path, "wb");
    if (!fout) {
        fprintf(stderr, "Cannot open output file: %s\n", output_path);
        return (2);
    }

    /* LZ4: prepend 4-byte little-endian original size header */
    if (is_lz4) {
        uint8_t hdr[4];
        hdr[0] = (uint8_t)(input_len & 0xFF);
        hdr[1] = (uint8_t)((input_len >> 8) & 0xFF);
        hdr[2] = (uint8_t)((input_len >> 16) & 0xFF);
        hdr[3] = (uint8_t)((input_len >> 24) & 0xFF);
        if (fwrite(hdr, 1, 4, fout) != 4) {
            fprintf(stderr, "Error writing LZ4 header\n");
            fclose(fout);
            remove(output_path);
            return (2);
        }
    }

    /* Buffered write errors can surface at fclose; never leave a partial
     * output file behind on failure. */
    if (fwrite(output_buf, 1, output_len, fout) != output_len ||
        fclose(fout) != 0) {
        fprintf(stderr, "Error writing output file\n");
        remove(output_path);
        return (2);
    }

    /* Print stats */
    file_size = is_lz4 ? output_len + 4 : output_len;
    printf("%s: %zu -> %zu bytes (%.1f%%)\n", algorithm, input_len, file_size,
           input_len > 0 ? (100.0 * file_size / input_len) : 0.0);

    return (0);
}
