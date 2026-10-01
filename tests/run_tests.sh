#!/bin/sh

set -u

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
VMM="$PROJECT_DIR/myvmm"
TMP_ROOT=${TMPDIR:-/tmp}
TMP_DIR=$(mktemp -d "$TMP_ROOT/myvmm-tests.XXXXXX") || exit 1
TEST_COUNT=0

cleanup()
{
    rm -rf "$TMP_DIR"
}

trap cleanup EXIT HUP INT TERM

fail()
{
    echo "FAIL: $*" >&2
    exit 1
}

pass()
{
    TEST_COUNT=$((TEST_COUNT + 1))
    echo "ok $TEST_COUNT - $1"
}

run_success()
{
    name=$1
    config=$2
    output=$3
    error_output="$output.err"

    if ! "$VMM" -v "$config" >"$output" 2>"$error_output"; then
        cat "$error_output" >&2
        fail "$name"
    fi

    pass "$name"
}

run_failure()
{
    name=$1
    config=$2
    expected_error=$3
    output="$TMP_DIR/failure.out"
    error_output="$TMP_DIR/failure.err"

    if "$VMM" -v "$config" >"$output" 2>"$error_output"; then
        fail "$name returned success"
    fi

    if ! grep -F "$expected_error" "$error_output" >/dev/null; then
        cat "$error_output" >&2
        fail "$name produced the wrong diagnostic"
    fi

    pass "$name"
}

assert_dump_format()
{
    output=$1
    name=$2

    if ! awk -F= '
        BEGIN { valid = 1 }
        {
            expected = "R" (NR - 1)
            if ($1 != expected || $2 !~ /^[0-9]+$/) {
                valid = 0
            }
        }
        END { exit !(valid && NR == 32) }
    ' "$output"; then
        fail "$name did not produce exactly R0 through R31"
    fi
}

assert_register()
{
    output=$1
    register=$2
    expected=$3
    name=$4
    actual=$(sed -n "s/^R${register}=//p" "$output")

    if [ "$actual" != "$expected" ]; then
        fail "$name: expected R$register=$expected, got R$register=$actual"
    fi
}

if [ ! -x "$VMM" ]; then
    fail "myvmm is not built; run make first"
fi

ARITHMETIC_OUTPUT="$TMP_DIR/arithmetic.out"
run_success "all arithmetic instructions" \
    "$PROJECT_DIR/test/arithmetic.conf" "$ARITHMETIC_OUTPUT"
assert_dump_format "$ARITHMETIC_OUTPUT" "arithmetic dump"
assert_register "$ARITHMETIC_OUTPUT" 8 12 "addi"
assert_register "$ARITHMETIC_OUTPUT" 9 4294967291 "negative addi"
assert_register "$ARITHMETIC_OUTPUT" 10 7 "add"
assert_register "$ARITHMETIC_OUTPUT" 11 7 "addu"
assert_register "$ARITHMETIC_OUTPUT" 12 15 "addiu"
assert_register "$ARITHMETIC_OUTPUT" 13 17 "sub"
assert_register "$ARITHMETIC_OUTPUT" 14 4294967284 "subu"
assert_register "$ARITHMETIC_OUTPUT" 15 4294967236 "mul"
assert_register "$ARITHMETIC_OUTPUT" 16 4294967295 "mult/mfhi"
assert_register "$ARITHMETIC_OUTPUT" 17 4294967236 "mult/mflo"
assert_register "$ARITHMETIC_OUTPUT" 18 2 "div/mfhi"
assert_register "$ARITHMETIC_OUTPUT" 19 4294967294 "div/mflo"
pass "arithmetic register results"

LOGICAL_OUTPUT="$TMP_DIR/logical.out"
run_success "all logical instructions" \
    "$PROJECT_DIR/test/logical.conf" "$LOGICAL_OUTPUT"
assert_dump_format "$LOGICAL_OUTPUT" "logical dump"
assert_register "$LOGICAL_OUTPUT" 0 0 "immutable zero"
assert_register "$LOGICAL_OUTPUT" 8 4294967295 "addiu sign extension"
assert_register "$LOGICAL_OUTPUT" 9 240 "ori"
assert_register "$LOGICAL_OUTPUT" 10 4080 "andi"
assert_register "$LOGICAL_OUTPUT" 11 4080 "or"
assert_register "$LOGICAL_OUTPUT" 12 240 "and"
assert_register "$LOGICAL_OUTPUT" 13 3840 "sll"
assert_register "$LOGICAL_OUTPUT" 14 15 "srl"
pass "logical register results"

REGISTER_OUTPUT="$TMP_DIR/registers.out"
run_success "named and numbered registers" \
    "$SCRIPT_DIR/configs/registers.conf" "$REGISTER_OUTPUT"
assert_dump_format "$REGISTER_OUTPUT" "register dump"
assert_register "$REGISTER_OUTPUT" 0 0 "write to zero"
assert_register "$REGISTER_OUTPUT" 30 9 "numbered register"
assert_register "$REGISTER_OUTPUT" 31 7 "ra register"
pass "register parsing results"

SAMPLE_OUTPUT="$TMP_DIR/sample.out"
run_success "supplied sample programs" \
    "$SCRIPT_DIR/configs/sample_vm1.conf" "$SAMPLE_OUTPUT"
assert_dump_format "$SAMPLE_OUTPUT" "sample VM 1 dump"
assert_register "$SAMPLE_OUTPUT" 3 12 "li positive immediate"
assert_register "$SAMPLE_OUTPUT" 11 107 "xor immediate"

SAMPLE_2_OUTPUT="$TMP_DIR/sample-2.out"
run_success "second supplied sample program" \
    "$SCRIPT_DIR/configs/sample_vm2.conf" "$SAMPLE_2_OUTPUT"
assert_dump_format "$SAMPLE_2_OUTPUT" "sample VM 2 dump"
assert_register "$SAMPLE_2_OUTPUT" 3 4294967291 "li negative immediate"
assert_register "$SAMPLE_2_OUTPUT" 10 30 "xor register"
assert_register "$SAMPLE_2_OUTPUT" 11 116 "or immediate"
pass "sample instruction results"

if ! (cd "$TMP_DIR" && "$VMM" -v "$PROJECT_DIR/test/arithmetic.conf" \
    >"$TMP_DIR/other-directory.out" 2>"$TMP_DIR/other-directory.err"); then
    cat "$TMP_DIR/other-directory.err" >&2
    fail "config-relative path from another working directory"
fi
pass "config-relative path from another working directory"

printf '%s\n' \
    'vm_exec_slice_in_instructions=2' \
    "vm_binary=$SCRIPT_DIR/programs/registers.asm" \
    >"$TMP_DIR/absolute.conf"
run_success "absolute binary path" "$TMP_DIR/absolute.conf" \
    "$TMP_DIR/absolute.out"

printf 'addi $t0, $zero, 1\r\nDUMP_PROCESSOR_STATE\r\n' \
    >"$TMP_DIR/crlf.asm"
printf '%s\n' \
    'vm_exec_slice_in_instructions=1' \
    'vm_binary=crlf.asm' \
    >"$TMP_DIR/crlf.conf"
run_success "CRLF assembly input" "$TMP_DIR/crlf.conf" "$TMP_DIR/crlf.out"
assert_dump_format "$TMP_DIR/crlf.out" "CRLF dump"
assert_register "$TMP_DIR/crlf.out" 8 1 "CRLF instruction"

if ! "$VMM" -v "$PROJECT_DIR/test/arithmetic.conf" \
    -v "$PROJECT_DIR/test/logical.conf" >"$TMP_DIR/multiple.out" \
    2>"$TMP_DIR/multiple.err"; then
    cat "$TMP_DIR/multiple.err" >&2
    fail "multiple VM invocation"
fi
if [ "$(wc -l <"$TMP_DIR/multiple.out" | tr -d ' ')" -ne 64 ]; then
    fail "multiple VM invocation did not produce two dumps"
fi
pass "multiple VM invocation"

run_failure "division by zero" "$SCRIPT_DIR/configs/divide_by_zero.conf" \
    "Division by zero"
run_failure "signed arithmetic overflow" "$SCRIPT_DIR/configs/overflow.conf" \
    "Arithmetic overflow"
run_failure "unknown instruction" "$SCRIPT_DIR/configs/invalid_instruction.conf" \
    "Unknown instruction"
run_failure "zero execution slice" "$SCRIPT_DIR/configs/invalid_slice.conf" \
    "invalid value for vm_exec_slice_in_instructions"
run_failure "unknown configuration key" "$SCRIPT_DIR/configs/unknown_key.conf" \
    "unknown key"

echo "All $TEST_COUNT tests passed."
