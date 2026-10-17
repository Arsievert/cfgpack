#!/bin/bash
#
# Run all test binaries and produce a summary.
# Full output is written to build/test.log
#
# Works from any directory: tests open fixtures relative to the repo root,
# so the script changes there first.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_OUT="$ROOT_DIR/build/out"
LOG_FILE="$ROOT_DIR/build/test.log"

cd "$ROOT_DIR"

# Test binaries to run (order matters for readability)
TESTS=(basic core_edge coverage decompress io_edge io_littlefs json_edge json_remap measure msgpack msgpack_decode msgpack_schema null_args parser_bounds parser runtime)

# Per-binary wall-clock limit, so a hang fails instead of blocking CI.
TIMEOUT_SECS=120
TIMEOUT_CMD=""
if command -v timeout >/dev/null 2>&1; then
    TIMEOUT_CMD="timeout $TIMEOUT_SECS"
elif command -v gtimeout >/dev/null 2>&1; then
    TIMEOUT_CMD="gtimeout $TIMEOUT_SECS"
fi

# Colors
RED='\033[31m'
GREEN='\033[32m'
RESET='\033[0m'

mkdir -p "$ROOT_DIR/build" "$ROOT_DIR/build/tmp"
> "$LOG_FILE"

echo "Running tests..."
echo ""

total_passed=0
total_failed=0
any_failed=0

for test in "${TESTS[@]}"; do
    bin="$BUILD_OUT/$test"

    if [[ ! -x "$bin" ]]; then
        printf "  %-15s ${RED}MISSING${RESET} (run 'make tests')\n" "$test:"
        any_failed=1
        continue
    fi

    echo "=== $test ===" >> "$LOG_FILE"
    set +e
    output=$($TIMEOUT_CMD "$bin" 2>&1)
    exit_code=$?
    set -e
    echo "$output" >> "$LOG_FILE"
    echo "" >> "$LOG_FILE"

    # One line per case: "PASS name" / "FAIL name", after stripping colour
    # codes.  Anchoring excludes the indented CHECK diagnostics and the
    # "ALL PASS" / "SOME FAIL" summary lines, so each case counts once.
    plain=$(printf '%s\n' "$output" | sed $'s/\x1b\\[[0-9;]*m//g')
    passed=$(printf '%s\n' "$plain" | grep -c '^PASS ' || true)
    failed=$(printf '%s\n' "$plain" | grep -c '^FAIL ' || true)
    total=$((passed + failed))

    total_passed=$((total_passed + passed))
    total_failed=$((total_failed + failed))

    if [[ $exit_code -eq 124 ]]; then
        printf "  %-15s ${RED}TIMEOUT after %ds${RESET}\n" "$test:" "$TIMEOUT_SECS"
        any_failed=1
    elif [[ $failed -gt 0 || $exit_code -ne 0 ]]; then
        printf "  %-15s ${RED}%d/%d passed (%d FAILED)${RESET}\n" "$test:" "$passed" "$total" "$failed"
        any_failed=1
    else
        printf "  %-15s ${GREEN}%d/%d passed${RESET}\n" "$test:" "$passed" "$total"
    fi
done

echo ""
total=$((total_passed + total_failed))

if [[ $any_failed -eq 1 ]]; then
    printf "${RED}TOTAL: %d/%d passed (%d failed)${RESET}\n" "$total_passed" "$total" "$total_failed"
else
    printf "${GREEN}TOTAL: %d/%d passed${RESET}\n" "$total_passed" "$total"
fi

echo "Full log: build/test.log"

exit $any_failed
