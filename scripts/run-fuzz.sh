#!/bin/sh
#
# Run all fuzz targets with their seed corpora.
#
# Usage:
#   scripts/run-fuzz.sh              # 60s per target (default)
#   scripts/run-fuzz.sh 300          # 300s per target
#   scripts/run-fuzz.sh 0            # run indefinitely (Ctrl-C to stop)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_OUT="$ROOT_DIR/build/out"

DURATION="${1:-60}"
MAX_LEN=4096

# Colors
RED='\033[31m'
GREEN='\033[32m'
BOLD='\033[1m'
RESET='\033[0m'

# Targets and their corpus directories (parallel lists, same order)
TARGETS="fuzz_parse_map fuzz_parse_json fuzz_parse_msgpack fuzz_parse_msgpack_mutator fuzz_pagein fuzz_msgpack_decode"
CORPORA="corpus_map corpus_json corpus_msgpack corpus_msgpack_mutator corpus_pagein corpus_decode"

# Check that fuzz binaries exist
missing=0
for target in $TARGETS; do
    bin="$BUILD_OUT/$target"
    if [ ! -x "$bin" ]; then
        printf "${RED}Missing:${RESET} %s (run 'make fuzz' first)\n" "$bin"
        missing=1
    fi
done

if [ $missing -eq 1 ]; then
    echo ""
    echo "Build fuzz targets first:  make fuzz"
    exit 1
fi

echo ""
printf "${BOLD}libFuzzer sweep — %s seconds per target, max_len=%d${RESET}\n" \
    "$DURATION" "$MAX_LEN"
echo ""

any_failed=0

# Walk both lists in lockstep using set/shift
saved_targets="$TARGETS"
saved_corpora="$CORPORA"
set -- $saved_corpora
for target in $saved_targets; do
    corpus_dir="$1"; shift
    bin="$BUILD_OUT/$target"
    corpus="$ROOT_DIR/tests/fuzz/$corpus_dir"

    printf "${BOLD}> %-25s${RESET} corpus=tests/fuzz/%s\n" "$target" "$corpus_dir"

    set +e
    "$bin" "$corpus" \
        -max_total_time="$DURATION" \
        -max_len="$MAX_LEN" \
        -print_final_stats=1 \
        2>&1 | tail -20
    rc=$?
    set -e

    if [ $rc -eq 0 ]; then
        printf "  ${GREEN}OK${RESET}\n\n"
    else
        printf "  ${RED}FAILED (exit %d)${RESET}\n\n" "$rc"
        any_failed=1
    fi
done

if [ $any_failed -eq 1 ]; then
    printf "${RED}Some fuzz targets failed!${RESET}\n"
else
    printf "${GREEN}All fuzz targets completed successfully.${RESET}\n"
fi

exit $any_failed
