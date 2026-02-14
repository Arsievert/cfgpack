#!/bin/bash
#
# Run all test binaries and produce a summary.
# Full output is written to build/test.log
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_OUT="$ROOT_DIR/build/out"
LOG_FILE="$ROOT_DIR/build/test.log"

# Test binaries to run (order matters for readability)
TESTS=(basic core_edge decompress io_edge json_edge json_remap measure msgpack parser_bounds parser runtime)

# Colors
RED='\033[31m'
GREEN='\033[32m'
YELLOW='\033[33m'
RESET='\033[0m'

# Ensure build directory exists
mkdir -p "$ROOT_DIR/build"

# Clear log file
> "$LOG_FILE"

echo "Running tests..."
echo ""

total_passed=0
total_failed=0
any_failed=0

for test in "${TESTS[@]}"; do
    bin="$BUILD_OUT/$test"
    
    if [[ ! -x "$bin" ]]; then
        printf "  %-14s ${YELLOW}SKIP${RESET} (not found)\n" "$test:"
        continue
    fi
    
    # Run test and capture output
    echo "=== $test ===" >> "$LOG_FILE"
    set +e
    output=$("$bin" 2>&1)
    exit_code=$?
    set -e
    echo "$output" >> "$LOG_FILE"
    echo "" >> "$LOG_FILE"
    
    # Count PASS/FAIL lines (exclude "ALL PASS" summary line)
    passed=$(echo "$output" | grep "PASS" | grep -cv "ALL PASS" || true)
    failed=$(echo "$output" | grep -c "FAIL" || true)
    total=$((passed + failed))
    
    total_passed=$((total_passed + passed))
    total_failed=$((total_failed + failed))
    
    if [[ $failed -gt 0 || $exit_code -ne 0 ]]; then
        printf "  %-14s ${RED}%d/%d passed (%d FAILED)${RESET}\n" "$test:" "$passed" "$total" "$failed"
        any_failed=1
    else
        printf "  %-14s ${GREEN}%d/%d passed${RESET}\n" "$test:" "$passed" "$total"
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
