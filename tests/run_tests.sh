#!/bin/bash
# ─────────────────────────────────────────────────────
#  amalgame-net-curl — Test Runner
#  Usage: ./tests/run_tests.sh [path-to-amc]
#
#  Requires:
#    - libcurl4-openssl-dev installed (or equivalent)
#    - amc binary (path passed as arg, or autodetected)
# ─────────────────────────────────────────────────────
set -e

PKG_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TESTS_DIR="$PKG_DIR/tests"

AMC=""
if [ -n "$1" ]; then
    AMC="$1"
elif command -v amc >/dev/null 2>&1; then
    AMC="$(command -v amc)"
elif [ -x "$PKG_DIR/../Amalgame/amc" ]; then
    AMC="$PKG_DIR/../Amalgame/amc"
elif [ -x "$HOME/.local/bin/amc" ]; then
    AMC="$HOME/.local/bin/amc"
fi
if [ -z "$AMC" ] || [ ! -x "$AMC" ]; then
    echo "error: amc binary not found"
    exit 2
fi

RUNTIME_DIR=""
if [ -n "$AMC_RUNTIME" ] && [ -d "$AMC_RUNTIME" ]; then
    RUNTIME_DIR="$AMC_RUNTIME"
elif [ -d "$PKG_DIR/../Amalgame/runtime" ]; then
    RUNTIME_DIR="$PKG_DIR/../Amalgame/runtime"
elif [ -d "$HOME/.amalgame/runtime" ]; then
    RUNTIME_DIR="$HOME/.amalgame/runtime"
fi
if [ -z "$RUNTIME_DIR" ]; then
    echo "error: Amalgame runtime headers not found"
    exit 2
fi

BUILD_DIR=$(mktemp -d -t amalgame-net-curl-XXXXXX)
trap 'rm -rf "$BUILD_DIR"' EXIT

GREEN='\033[0;32m'; RED='\033[0;31m'; NC='\033[0m'

echo "Using amc: $AMC"
echo "Using runtime: $RUNTIME_DIR"
cd "$PKG_DIR"

# ── Smoke test — header compiles + links against libcurl ─────────
echo ""
echo "── Smoke test (header + libcurl link) ──"
cat > "$BUILD_DIR/smoke.c" <<'EOF'
#include "Amalgame_Net_Curl.h"
#include <stdio.h>
int main(void) {
    /* Build a no-op AmalgameHttpResponse via the stub-or-real path —
     * we don't actually hit the network. The goal is link-time
     * verification that all Amalgame_Net_Curl_* symbols resolve. */
    AmalgameHttpResponse* r = Amalgame_Net_Curl_Http_Get("");
    (void) r;
    printf("AMALGAME_HAS_CURL: %d\n",
#ifdef AMALGAME_HAS_CURL
        1
#else
        0
#endif
    );
    return 0;
}
EOF
gcc -O2 -I"$RUNTIME_DIR" -I"$PKG_DIR/runtime" \
    "$BUILD_DIR/smoke.c" \
    -lcurl -lgc \
    -o "$BUILD_DIR/smoke" 2>&1 | head -5
if [ ! -x "$BUILD_DIR/smoke" ]; then
    echo -e "${RED}FAIL${NC} (smoke build)"
    exit 1
fi
SMOKE_OUT=$("$BUILD_DIR/smoke")
echo "$SMOKE_OUT"
if echo "$SMOKE_OUT" | grep -q "AMALGAME_HAS_CURL: 1"; then
    echo -e "${GREEN}smoke test passed (libcurl detected + linked)${NC}"
else
    echo -e "${RED}FAIL${NC} (libcurl not detected)"
    exit 1
fi

# ── Live HTTP request test (against httpbin.org) ─────────────────
# Optional — skips if no network. Verifies the actual round-trip
# through curl_easy_perform works end-to-end against a real server.
echo ""
echo "── Live HTTP test (httpbin.org) ──"
cat > "$BUILD_DIR/live.c" <<'EOF'
#include "Amalgame_Net_Curl.h"
#include <stdio.h>
#include <string.h>
int main(void) {
    AmalgameHttpResponse* r = Amalgame_Net_Curl_Http_Get("https://httpbin.org/status/200");
    if (r->Status == 200 && r->Ok) {
        printf("LIVE HTTP: 200 OK\n");
        return 0;
    }
    if (r->Status == 0) {
        /* network error or no libcurl — skip */
        printf("LIVE HTTP: skipped (status 0: %s)\n", r->Error ? r->Error : "(no error)");
        return 0;
    }
    printf("LIVE HTTP FAIL: status=%lld\n", (long long) r->Status);
    return 1;
}
EOF
gcc -O2 -I"$RUNTIME_DIR" -I"$PKG_DIR/runtime" \
    "$BUILD_DIR/live.c" \
    -lcurl -lgc \
    -o "$BUILD_DIR/live" 2>&1 | head -3
if [ -x "$BUILD_DIR/live" ]; then
    "$BUILD_DIR/live"
fi

echo ""
echo -e "${GREEN}All tests completed${NC}"
