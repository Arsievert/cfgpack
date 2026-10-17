#!/bin/bash
#
# One-time setup for new clones: check dependencies, install hooks, verify build.
#
# Usage: ./scripts/setup.sh
#

set -e

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

# ── Colors ────────────────────────────────────────────────────────────────────
RED='\033[31m'
GREEN='\033[32m'
YELLOW='\033[33m'
BOLD='\033[1m'
RESET='\033[0m'

PASS=0
FAIL=0
WARN=0

pass() { printf "  ${GREEN}OK${RESET}  %s\n" "$1"; PASS=$((PASS + 1)); }
fail() { printf "  ${RED}FAIL${RESET}  %s\n" "$1"; FAIL=$((FAIL + 1)); }
warn() { printf "  ${YELLOW}WARN${RESET}  %s\n" "$1"; WARN=$((WARN + 1)); }

# ── 1. Required tools ────────────────────────────────────────────────────────
printf "${BOLD}Checking required tools...${RESET}\n"

for tool in clang clang-format make ar; do
    if command -v "$tool" >/dev/null 2>&1; then
        pass "$tool ($(command -v "$tool"))"
    else
        fail "$tool not found"
    fi
done

# ── 2. Optional tools ────────────────────────────────────────────────────────
printf "\n${BOLD}Checking optional tools...${RESET}\n"

# Fuzz testing requires libFuzzer, which Apple Clang doesn't ship.
# Check for Homebrew LLVM on macOS.
if [ "$(uname)" = "Darwin" ]; then
    BREW_LLVM="/opt/homebrew/opt/llvm/bin/clang"
    if [ -x "$BREW_LLVM" ]; then
        pass "Homebrew LLVM ($BREW_LLVM) — fuzz testing available"
    else
        warn "Homebrew LLVM not found — fuzz testing unavailable (brew install llvm)"
    fi
else
    # On Linux, check if the system clang supports -fsanitize=fuzzer by
    # linking a minimal harness (an empty input never links: libFuzzer
    # needs LLVMFuzzerTestOneInput).
    FUZZ_PROBE="$(mktemp -d)"
    printf '#include <stddef.h>\n#include <stdint.h>\nint LLVMFuzzerTestOneInput(const uint8_t *d, size_t n){(void)d;(void)n;return 0;}\n' \
        > "$FUZZ_PROBE/probe.c"
    if clang -fsanitize=fuzzer "$FUZZ_PROBE/probe.c" -o "$FUZZ_PROBE/probe" 2>/dev/null; then
        pass "libFuzzer support — fuzz testing available"
    else
        warn "libFuzzer not available — fuzz testing unavailable"
    fi
    rm -rf "$FUZZ_PROBE"
fi

# ── 3. Install pre-commit hook ───────────────────────────────────────────────
printf "\n${BOLD}Installing pre-commit hook...${RESET}\n"

# git rev-parse resolves the hooks directory correctly for worktrees,
# submodules (where .git is a file) and a configured core.hooksPath.
HOOKS_DIR="$(git -C "$ROOT_DIR" rev-parse --git-path hooks 2>/dev/null || true)"
if [ -n "$HOOKS_DIR" ]; then
    case "$HOOKS_DIR" in
        /*) ;;
        *) HOOKS_DIR="$ROOT_DIR/$HOOKS_DIR" ;;
    esac
    mkdir -p "$HOOKS_DIR"
    ln -sf "$ROOT_DIR/scripts/pre-commit" "$HOOKS_DIR/pre-commit"
    pass "pre-commit hook installed ($HOOKS_DIR/pre-commit)"
else
    fail "not a git repository — pre-commit hook not installed"
fi

# ── 4. Verify build + tests ──────────────────────────────────────────────────
printf "\n${BOLD}Verifying build and tests...${RESET}\n"

if make -C "$ROOT_DIR" tests >/dev/null 2>&1; then
    pass "build succeeded"
else
    fail "build failed — run 'make tests' for details"
fi

if "$ROOT_DIR/scripts/run-tests.sh" >/dev/null 2>&1; then
    pass "all tests passed"
else
    fail "tests failed — run 'scripts/run-tests.sh' for details"
fi

# ── Summary ───────────────────────────────────────────────────────────────────
printf "\n${BOLD}Setup complete:${RESET} ${GREEN}${PASS} passed${RESET}"
if [ "$WARN" -gt 0 ]; then printf ", ${YELLOW}${WARN} warnings${RESET}"; fi
if [ "$FAIL" -gt 0 ]; then printf ", ${RED}${FAIL} failed${RESET}"; fi
printf "\n"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
