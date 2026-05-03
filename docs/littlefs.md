# LittleFS Storage Wrappers

CFGPack provides optional LittleFS-based convenience wrappers for embedded systems with flash storage. These functions handle the file open/read/write/close lifecycle while preserving cfgpack's zero-heap-allocation guarantee.

Unlike the POSIX `io_file.h` wrappers (which use `FILE*` operations), the LittleFS wrappers use `lfs_file_opencfg()` with a caller-provided file cache buffer, making them compatible with `LFS_NO_MALLOC` builds.

To use these functions, compile with `-DCFGPACK_LITTLEFS` and link `src/io_littlefs.c` and the vendored LittleFS sources with your project.

## API

```c
#include "cfgpack/io_littlefs.h"

/* Encode context to a LittleFS file using caller scratch buffer (no heap).
 * scratch must be >= cfg->cache_size + encoded size. */
cfgpack_err_t cfgpack_pageout_lfs(const cfgpack_ctx_t *ctx,
                                  lfs_t *lfs,
                                  const char *path,
                                  uint8_t *scratch,
                                  size_t scratch_cap);

/* Decode context from a LittleFS file using caller scratch buffer (no heap).
 * scratch must be >= cfg->cache_size + file size. */
cfgpack_err_t cfgpack_pagein_lfs(cfgpack_ctx_t *ctx,
                                 lfs_t *lfs,
                                 const char *path,
                                 uint8_t *scratch,
                                 size_t scratch_cap);
```

Parameters:

- `ctx` — Initialized cfgpack context.
- `lfs` — Mounted LittleFS instance (caller-owned). The caller is responsible for mounting and unmounting.
- `path` — File path within the LittleFS filesystem.
- `scratch` — Scratch buffer that serves double duty (see layout below).
- `scratch_cap` — Total capacity of the scratch buffer in bytes.

## Scratch Buffer Layout

The scratch buffer is split internally into two regions:

```
scratch buffer:
┌─────────────────────────────┬──────────────────────────────────────┐
│  LittleFS file cache        │  serialized config data              │
│  [0 .. cache_size-1]        │  [cache_size .. scratch_cap-1]       │
└─────────────────────────────┴──────────────────────────────────────┘
```

- **First `cache_size` bytes**: Passed to `lfs_file_opencfg()` as `struct lfs_file_config.buffer`. This is the LittleFS file cache, sized to match `lfs->cfg->cache_size`.
- **Remaining bytes**: Used by `cfgpack_pageout()` / `cfgpack_pagein_buf()` for the serialized MessagePack data.

Minimum scratch size: `cache_size + max(encoded_config_size, stored_file_size)`.

If `scratch_cap <= cache_size`, the functions return `CFGPACK_ERR_BOUNDS` immediately. The `cache_size` value comes from your LittleFS configuration (`lfs->cfg->cache_size`), which is typically set to the flash block or page size.

This design avoids calling `lfs_malloc()`, so the wrappers work in `LFS_NO_MALLOC` builds without any changes.

## Usage Example

```c
#include "cfgpack/cfgpack.h"
#include "cfgpack/io_littlefs.h"

/* Assume lfs_cfg.cache_size == 256 */
#define SCRATCH_SIZE (256 + 1024)  /* cache + room for encoded config */
static uint8_t scratch[SCRATCH_SIZE];

lfs_t lfs;
/* ... mount LittleFS (caller-owned) ... */
lfs_mount(&lfs, &lfs_cfg);

/* Write config to flash */
cfgpack_err_t err = cfgpack_pageout_lfs(&ctx, &lfs, "/config.bin",
                                         scratch, sizeof(scratch));
if (err != CFGPACK_OK) { /* handle error */ }

/* Read config back from flash */
err = cfgpack_pagein_lfs(&ctx, &lfs, "/config.bin",
                          scratch, sizeof(scratch));
if (err != CFGPACK_OK) { /* handle error */ }

lfs_unmount(&lfs);
```

## Composable I/O Pattern

`cfgpack_pagein_lfs()` wraps `cfgpack_pagein_buf()` only — it does **not** wrap `cfgpack_pagein_remap()`. This means it loads data directly into the current schema without any index remapping or type widening.

For **cross-version migration**, you need to manually read the raw blob from LittleFS, detect the schema version, and call `cfgpack_pagein_remap()` yourself:

```c
/* 1. Split scratch the same way the wrappers do internally */
lfs_size_t cache_size = lfs.cfg->cache_size;
uint8_t *file_cache = scratch;
uint8_t *data_buf   = scratch + cache_size;
size_t   data_cap   = sizeof(scratch) - cache_size;

/* 2. Read raw blob from LittleFS */
struct lfs_file_config fcfg = {.buffer = file_cache};
lfs_file_t file;
lfs_file_opencfg(&lfs, &file, "/config.bin", LFS_O_RDONLY, &fcfg);
lfs_ssize_t size = lfs_file_size(&lfs, &file);
lfs_file_read(&lfs, &file, data_buf, size);
lfs_file_close(&lfs, &file);

/* 3. Detect schema version */
char name[64];
cfgpack_peek_name(data_buf, (size_t)size, name, sizeof(name));

/* 4. Apply remap if needed */
if (strcmp(name, "sensor_v1") == 0) {
    cfgpack_pagein_remap(&ctx_v2, data_buf, (size_t)size,
                         v1_to_v2_remap, remap_count);
} else {
    cfgpack_pagein_buf(&ctx_v2, data_buf, (size_t)size);
}
```

See `examples/flash_config/` for a complete working example of this pattern.

## Building

`src/io_littlefs.c` is **not** included in the core `libcfgpack.a` — like `io_file.c`, it is compiled separately to keep the core library free of filesystem dependencies. To use it:

```bash
$(CC) -DCFGPACK_LITTLEFS -DCFGPACK_HOSTED \
    -Iinclude -Ithird_party/littlefs \
    myapp.c src/io_littlefs.c \
    third_party/littlefs/lfs.c third_party/littlefs/lfs_util.c \
    -Lbuild/out -lcfgpack -o myapp
```

For embedded builds, omit `-DCFGPACK_HOSTED` and adjust the toolchain as needed.

## Error Codes

| Return | Condition |
|--------|-----------|
| `CFGPACK_OK` | Success |
| `CFGPACK_ERR_BOUNDS` | `scratch_cap <= cache_size` (no room for data) |
| `CFGPACK_ERR_ENCODE` | Data portion too small for `cfgpack_pageout()` (pageout only) |
| `CFGPACK_ERR_IO` | LittleFS file open, read, write, or size failure |
| `CFGPACK_ERR_CRC` | CRC-32C integrity check failed (pagein only) |
| `CFGPACK_ERR_DECODE` | Invalid MessagePack payload (pagein only) |

## Working Example

The `examples/flash_config/` example demonstrates a complete LittleFS + LZ4 + schema migration workflow:

- **RAM-backed LittleFS block device** — runs on any desktop without real flash hardware
- **LZ4-compressed msgpack binary schemas** — built by a pipeline: `.map` → `cfgpack-schema-pack` → `.msgpack` → `cfgpack-compress lz4` → `.msgpack.lz4`
- **Schema migration v1 → v2** using `cfgpack_pagein_remap()` with all five migration scenarios: keep, widen, move, remove, and add
- **Composable I/O pattern** — manual LFS read followed by `cfgpack_pagein_remap()` for cross-version migration

```bash
cd examples/flash_config && make run
```

## Third-Party Library

LittleFS is vendored in `third_party/littlefs/` for self-contained builds:

```
third_party/littlefs/
    lfs.h, lfs.c            # Core filesystem implementation
    lfs_util.h, lfs_util.c  # Utility functions
    LICENSE.md               # BSD-3-Clause license
```

[LittleFS](https://github.com/littlefs-project/littlefs) is a little fail-safe filesystem designed for microcontrollers. It provides power-loss resilience, wear leveling, and bounded RAM/ROM usage — making it a natural companion for cfgpack's zero-allocation architecture.
