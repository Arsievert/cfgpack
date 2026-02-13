# Compression Support

CFGPack supports decompression of stored configs using LZ4 or heatshrink algorithms. This is useful when config blobs are stored compressed in flash to save space. Compression must be done externally (e.g., at build time or on the host); only decompression is supported on the device.

Both LZ4 and heatshrink are enabled by default. To disable for minimal embedded builds, edit the `CFLAGS` in the Makefile to remove `-DCFGPACK_LZ4` and/or `-DCFGPACK_HEATSHRINK`.

## API

```c
#include "cfgpack/decompress.h"

/* Decompress LZ4 data and load into context.
 * decompressed_size must be known (stored alongside compressed data).
 * scratch/scratch_cap: caller-provided buffer for decompressed output. */
cfgpack_err_t cfgpack_pagein_lz4(cfgpack_ctx_t *ctx, const uint8_t *data, size_t len,
                                  size_t decompressed_size,
                                  uint8_t *scratch, size_t scratch_cap);

/* Decompress heatshrink data and load into context.
 * Encoder must use window=8, lookahead=4 to match decoder config.
 * scratch/scratch_cap: caller-provided buffer for decompressed output. */
cfgpack_err_t cfgpack_pagein_heatshrink(cfgpack_ctx_t *ctx, const uint8_t *data, size_t len,
                                         uint8_t *scratch, size_t scratch_cap);
```

## Usage Example

```c
// LZ4 example - decompressed size must be stored with the compressed blob
uint8_t compressed[2048];
uint8_t scratch[4096];  // caller-provided decompression buffer
size_t compressed_len = read_from_flash(compressed);
size_t decompressed_size = read_size_header();  // stored alongside blob

cfgpack_err_t err = cfgpack_pagein_lz4(&ctx, compressed, compressed_len, decompressed_size,
                                        scratch, sizeof(scratch));
if (err != CFGPACK_OK) {
    // Handle decompression or decode error
}

// Heatshrink example - no size header needed (streaming)
err = cfgpack_pagein_heatshrink(&ctx, compressed, compressed_len, scratch, sizeof(scratch));
```

## Implementation Notes

- **Caller-provided buffer**: Both decompression functions accept a `scratch` / `scratch_cap` parameter for the decompressed output. The caller controls the maximum decompressed size.
- **LZ4 path**: Fully reentrant — no static state.
- **Heatshrink path**: Uses a static decoder instance and is NOT thread-safe. The LZ4 path has no such limitation.
- **Heatshrink parameters**: The decoder is configured with window=8 bits (256 bytes) and lookahead=4 bits (16 bytes). The encoder must use matching parameters.
- **Vendored sources**: LZ4 and heatshrink source files are vendored in `third_party/` to avoid external dependencies.

## Compression Workflow

A typical workflow for storing compressed config:

1. **At build time or on host**: Serialize config with `cfgpack_pageout()`, compress with LZ4 or heatshrink, store compressed blob + size metadata in flash image
2. **On device boot**: Read compressed blob from flash, decompress with `cfgpack_pagein_lz4()` or `cfgpack_pagein_heatshrink()`

## Compression Tool

The `cfgpack-compress` CLI tool compresses files for build-time or host-side use:

```bash
make tools
./build/out/cfgpack-compress <algorithm> <input> <output>
```

Where `<algorithm>` is either `lz4` or `heatshrink`.

Example:
```bash
# Compress a serialized config blob
./build/out/cfgpack-compress lz4 config.bin config.lz4
./build/out/cfgpack-compress heatshrink config.bin config.hs
```

## Third-Party Libraries

LZ4 and heatshrink sources are vendored in `third_party/` for self-contained builds:

```
third_party/
  lz4/
    lz4.h, lz4.c              # BSD-2-Clause license
  heatshrink/
    heatshrink_config.h       # window=8, lookahead=4
    heatshrink_decoder.h/c    # Used by library
    heatshrink_encoder.h/c    # Used by compression tool and tests
                              # ISC license
```
