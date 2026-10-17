#!/bin/bash
#
# Smoke-test the CLI tools end to end against the checked-in fixtures.
#
# Usage: scripts/test-tools.sh      (after `make tools`)
# Exit:  0 if every step behaves as expected, 1 otherwise.
#

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
OUT="$ROOT_DIR/build/out"
TMP="$ROOT_DIR/build/tmp/tools"

PACK="$OUT/cfgpack-schema-pack"
VALIDATE="$OUT/cfgpack-schema-validate"
COMPRESS="$OUT/cfgpack-compress"

RED='\033[31m'
GREEN='\033[32m'
RESET='\033[0m'

cd "$ROOT_DIR"
mkdir -p "$TMP"
fails=0

# expect <expected-exit> <description> <command...>
expect() {
    local want="$1"; shift
    local desc="$1"; shift
    "$@" >"$TMP/last.out" 2>&1
    local got=$?
    if [ "$got" -eq "$want" ]; then
        printf "  ${GREEN}OK${RESET}    %s\n" "$desc"
    else
        printf "  ${RED}FAIL${RESET}  %s (exit %d, expected %d)\n" "$desc" "$got" "$want"
        sed 's/^/        /' "$TMP/last.out"
        fails=$((fails + 1))
    fi
}

for bin in "$PACK" "$VALIDATE" "$COMPRESS"; do
    if [ ! -x "$bin" ]; then
        printf "${RED}Missing:${RESET} %s (run 'make tools' first)\n" "$bin"
        exit 1
    fi
done

echo "Tool smoke tests:"

# --- schema-pack ----------------------------------------------------------
expect 0 "pack .map -> msgpack"            "$PACK" tests/data/sample.map "$TMP/sample.bin"
expect 1 "pack: usage error without args"  "$PACK"
expect 2 "pack: missing input file"        "$PACK" tests/data/does_not_exist.map "$TMP/x.bin"
expect 3 "pack: invalid schema"            "$PACK" tests/data/bad_type.map "$TMP/x.bin"

# --- schema-validate -------------------------------------------------------
expect 0 "validate .map"                   "$VALIDATE" tests/data/sample.map
expect 0 "validate packed msgpack"         "$VALIDATE" "$TMP/sample.bin"
expect 0 "validate with --format override" "$VALIDATE" --format msgpack "$TMP/sample.bin"
expect 3 "validate: invalid schema"        "$VALIDATE" tests/data/bad_type.map
expect 3 "validate: duplicate index"       "$VALIDATE" tests/data/dup_index.map
expect 2 "validate: missing file"          "$VALIDATE" tests/data/does_not_exist.map
expect 1 "validate: unknown option"        "$VALIDATE" --bogus tests/data/sample.map
expect 1 "validate: both compressors"      "$VALIDATE" --lz4 --heatshrink "$TMP/sample.bin"
: > "$TMP/empty.map"
expect 3 "validate: empty file"            "$VALIDATE" "$TMP/empty.map"

# --- compress + validate round trip -----------------------------------------
expect 0 "compress lz4"                    "$COMPRESS" lz4 "$TMP/sample.bin" "$TMP/sample.lz4"
expect 0 "validate --lz4"                  "$VALIDATE" --lz4 "$TMP/sample.lz4"
expect 0 "compress heatshrink"             "$COMPRESS" heatshrink "$TMP/sample.bin" "$TMP/sample.hs"
expect 0 "validate --heatshrink"           "$VALIDATE" --heatshrink "$TMP/sample.hs"
expect 1 "compress: unknown algorithm"     "$COMPRESS" zip "$TMP/sample.bin" "$TMP/x"
expect 2 "compress: missing input"         "$COMPRESS" lz4 "$TMP/nope.bin" "$TMP/x"
expect 3 "validate --lz4 on truncated data" "$VALIDATE" --lz4 "$TMP/empty.map"

# Incompressible input near the size limit must still compress (output
# buffer used to be smaller than LZ4_compressBound).
head -c 65536 /dev/urandom > "$TMP/random.bin"
expect 0 "compress lz4 on 64 KB random input" "$COMPRESS" lz4 "$TMP/random.bin" "$TMP/random.lz4"
expect 0 "compress heatshrink on 64 KB random input" "$COMPRESS" heatshrink "$TMP/random.bin" "$TMP/random.hs"

echo ""
if [ "$fails" -eq 0 ]; then
    printf "${GREEN}All tool checks passed.${RESET}\n"
    exit 0
fi
printf "${RED}%d tool check(s) failed.${RESET}\n" "$fails"
exit 1
