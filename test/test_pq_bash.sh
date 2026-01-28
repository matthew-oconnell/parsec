#!/bin/bash
# Bash integration tests for PathParser (Phase 1)
# Tests path parsing from shell scripting perspective

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_PARSER="${SCRIPT_DIR}/../build/src/pq_test_parser"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

TESTS_PASSED=0
TESTS_FAILED=0

# Test helper function
assert_output() {
    local test_name="$1"
    local input="$2"
    local expected="$3"
    
    local output
    output=$($TEST_PARSER "$input" 2>&1) || {
        echo -e "${RED}✗${NC} $test_name (parser failed)"
        echo "  Input: $input"
        ((TESTS_FAILED++))
        return
    }
    
    if [ "$output" = "$expected" ]; then
        echo -e "${GREEN}✓${NC} $test_name"
        ((TESTS_PASSED++))
    else
        echo -e "${RED}✗${NC} $test_name"
        echo "  Input:    $input"
        echo "  Expected:"
        echo "$expected" | sed 's/^/    /'
        echo "  Got:"
        echo "$output" | sed 's/^/    /'
        ((TESTS_FAILED++))
    fi
}

# Test helper for errors
assert_error() {
    local test_name="$1"
    local input="$2"
    
    $TEST_PARSER "$input" >/dev/null 2>&1
    local exit_code=$?
    
    if [ $exit_code -eq 0 ]; then
        echo -e "${RED}✗${NC} $test_name (expected error but succeeded)"
        ((TESTS_FAILED++))
    else
        echo -e "${GREEN}✓${NC} $test_name"
        ((TESTS_PASSED++))
    fi
}

echo "=== PathParser Bash Integration Tests ==="
echo

# Phase 1.1 - Simple Path Tests
echo "Phase 1.1: Simple Paths"
assert_output "Simple two-segment path" "server/port" "key:server
key:port"

assert_output "Multi-segment path" "a/b/c/d" "key:a
key:b
key:c
key:d"

assert_output "Single segment" "single" "key:single"

assert_output "Path with spaces in keys" "server config/port number" "key:server config
key:port number"

assert_error "Empty path" ""
assert_error "Leading slash" "/leading/slash"
assert_error "Trailing slash" "trailing/slash/"
assert_error "Double slash" "double//slash"

echo

# Phase 1.2 - Array Index Tests
echo "Phase 1.2: Array Indices"
assert_output "Array index at end" "users/0" "key:users
index:0"

assert_output "Array index in middle" "users/0/name" "key:users
index:0
key:name"

assert_output "Multiple array indices" "items/10/nested/0" "key:items
index:10
key:nested
index:0"

assert_output "Large index" "arr/999" "key:arr
index:999"

assert_error "Negative index" "arr/-1"

assert_output "Numeric-looking string" "obj/123abc" "key:obj
key:123abc"

echo

# Phase 1.3 - Wildcard Tests
echo "Phase 1.3: Wildcards"
assert_output "Wildcard at end" "users/*" "key:users
wildcard:*"

assert_output "Wildcard in middle" "users/*/name" "key:users
wildcard:*
key:name"

assert_output "Multiple wildcards" "a/*/b/*/c" "key:a
wildcard:*
key:b
wildcard:*
key:c"

assert_output "Standalone wildcard" "*" "wildcard:*"

echo

# Complex Real-World Examples
echo "Complex Real-World Paths"
assert_output "CFD config path" "turbulence model/k-epsilon/constants" "key:turbulence model
key:k-epsilon
key:constants"

assert_output "Nested regions" "regions/0/boundary conditions/inlet/velocity" "key:regions
index:0
key:boundary conditions
key:inlet
key:velocity"

assert_output "Wildcard for all users" "users/*/email address" "key:users
wildcard:*
key:email address"

assert_output "Mixed complex path" "simulation data/timesteps/*/results/0/temperature" "key:simulation data
key:timesteps
wildcard:*
key:results
index:0
key:temperature"

echo
echo "==================================="
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
echo "==================================="

if [ $TESTS_FAILED -gt 0 ]; then
    exit 1
fi

exit 0
