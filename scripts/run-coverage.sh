#!/bin/bash
#
# Build with LLVM source-based coverage, run all tests, and generate a report.
#
# Outputs:
#   build/coverage/report.txt   — per-file line coverage summary
#   build/coverage/html/        — annotated source HTML (open index.html)
#
# Usage:
#   make coverage               # full pipeline (clean + build + report)
#   scripts/run-coverage.sh     # run directly (assumes instrumented binaries)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_OUT="$ROOT_DIR/build/out"
COV_DIR="$ROOT_DIR/build/coverage"

# Colors
RED='\033[31m'
GREEN='\033[32m'
BOLD='\033[1m'
RESET='\033[0m'

# ── Locate LLVM tools ────────────────────────────────────────────────────────
PROFDATA=""
LLVM_COV=""

if command -v llvm-profdata >/dev/null 2>&1; then
    PROFDATA="llvm-profdata"
    LLVM_COV="llvm-cov"
elif [ -x "/opt/homebrew/opt/llvm/bin/llvm-profdata" ]; then
    PROFDATA="/opt/homebrew/opt/llvm/bin/llvm-profdata"
    LLVM_COV="/opt/homebrew/opt/llvm/bin/llvm-cov"
else
    printf "${RED}Error: llvm-profdata not found.${RESET}\n"
    printf "Install with: brew install llvm (macOS) or apt install llvm (Linux)\n"
    exit 1
fi

# ── Test binaries ─────────────────────────────────────────────────────────────
TESTS=(basic core_edge coverage decompress io_edge io_littlefs json_edge json_remap measure msgpack msgpack_decode msgpack_schema null_args parser_bounds parser runtime)

mkdir -p "$COV_DIR"

# ── Run tests with per-binary profile output ──────────────────────────────────
printf "${BOLD}Running instrumented tests...${RESET}\n"

for test in "${TESTS[@]}"; do
    bin="$BUILD_OUT/$test"
    if [[ ! -x "$bin" ]]; then
        printf "  %-15s SKIP (not found)\n" "$test:"
        continue
    fi
    LLVM_PROFILE_FILE="$COV_DIR/%p-$test.profraw" "$bin" >/dev/null 2>&1
    printf "  %-15s ${GREEN}OK${RESET}\n" "$test:"
done

# ── Merge profiles ────────────────────────────────────────────────────────────
printf "\n${BOLD}Merging profiles...${RESET}\n"
"$PROFDATA" merge -sparse "$COV_DIR"/*.profraw -o "$COV_DIR/merged.profdata"

# ── Build object list for llvm-cov (all test binaries as sources) ─────────────
OBJECT_ARGS=""
FIRST_BIN=""
for test in "${TESTS[@]}"; do
    bin="$BUILD_OUT/$test"
    if [[ -x "$bin" ]]; then
        if [[ -z "$FIRST_BIN" ]]; then
            FIRST_BIN="$bin"
        else
            OBJECT_ARGS="$OBJECT_ARGS -object=$bin"
        fi
    fi
done

# ── Generate text summary ─────────────────────────────────────────────────────
printf "${BOLD}Generating reports...${RESET}\n"
"$LLVM_COV" report "$FIRST_BIN" $OBJECT_ARGS \
    -instr-profile="$COV_DIR/merged.profdata" \
    -ignore-filename-regex='third_party|tests' \
    > "$COV_DIR/report.txt"

# ── Generate HTML ─────────────────────────────────────────────────────────────
"$LLVM_COV" show "$FIRST_BIN" $OBJECT_ARGS \
    -instr-profile="$COV_DIR/merged.profdata" \
    -ignore-filename-regex='third_party|tests' \
    -format=html \
    -output-dir="$COV_DIR/html"

# ── Print summary ─────────────────────────────────────────────────────────────
printf "\n${BOLD}Coverage summary (library code only):${RESET}\n\n"
cat "$COV_DIR/report.txt"
printf "\n${GREEN}HTML report:${RESET} $COV_DIR/html/index.html\n"
