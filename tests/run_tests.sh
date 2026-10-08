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
    run_args_failure "$name" "$expected_error" -v "$config"
}

run_args_failure()
{
    name=$1
    expected_error=$2
    shift 2
    output="$TMP_DIR/failure.out"
    error_output="$TMP_DIR/failure.err"

    if "$VMM" "$@" >"$output" 2>"$error_output"; then
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

# Snapshot paths are relative to the working directory, not the config directory.
cd "$TMP_DIR" || fail "cannot enter temporary directory"
run_success "snapshot creation and continued execution" "$SCRIPT_DIR/configs/snapshot_vm1.conf" "$TMP_DIR/snapshot.out"
# Reuse that configuration for the subsequent restore and failure checks.
printf '%s\n' 'vm_exec_slice_in_instructions=2' \
    "vm_binary=$SCRIPT_DIR/programs/snapshot_vm1.asm" >snapshot.conf
[ -f checkpoint.snapshot ] && [ -f second.snapshot ] || fail "snapshot files missing"
cmp checkpoint.snapshot "$SCRIPT_DIR/snapshots/snapshot_vm1.snapshot" || fail "example snapshot differs from generated state"
pass "checked-in example snapshot matches generated snapshot"
sed -n '1,32p' snapshot.out | cut -d= -f2 >expected-registers
sed -n '5,36p' checkpoint.snapshot >saved-registers
cmp expected-registers saved-registers || fail "snapshot did not save all registers"
[ "$(sed -n '2p' checkpoint.snapshot)" = 144 ] || fail "wrong next PC"
[ "$(sed -n '3p' checkpoint.snapshot)" = 4294967295 ] || fail "wrong saved HI"
[ "$(sed -n '4p' checkpoint.snapshot)" = 4294967275 ] || fail "wrong saved LO"
pass "snapshot contains full CPU state and next PC"

mv checkpoint.snapshot resume.snapshot
mkdir checkpoint.snapshot
if ! "$VMM" -v snapshot.conf -s resume.snapshot >restored.out 2>restored.err; then
    cat restored.err >&2
    fail "snapshot restore"
fi
sed -n '33,64p' snapshot.out >expected-restored.out
cmp expected-restored.out restored.out || fail "restored execution differs from uninterrupted execution"
assert_dump_format restored.out "restored dump"
assert_register restored.out 16 4294967295 "restored HI"
assert_register restored.out 17 4294967275 "restored LO"
pass "restore skips prior instructions and snapshot, preserves all registers and HI/LO"

if ! "$VMM" -v snapshot.conf -s resume.snapshot \
    -v "$SCRIPT_DIR/configs/registers.conf" >mixed.out 2>mixed.err; then
    cat mixed.err >&2
    fail "mixed restored and fresh VMs"
fi
sed -n '1,32p' mixed.out >mixed-fresh.out
sed -n '33,64p' mixed.out >mixed-restored.out
cmp "$REGISTER_OUTPUT" mixed-fresh.out || fail "fresh VM inherited snapshot state"
cmp restored.out mixed-restored.out || fail "restored VM changed in mixed invocation"
[ "$(wc -l <mixed.out | tr -d ' ')" = 64 ] || fail "wrong mixed output length"
pass "per-VM snapshot association and isolation"

if ! "$VMM" -v "$SCRIPT_DIR/configs/registers.conf" -v snapshot.conf -s resume.snapshot \
    >mixed-reversed.out 2>mixed-reversed.err; then
    cat mixed-reversed.err >&2
    fail "snapshot on second VM"
fi
cmp mixed.out mixed-reversed.out || fail "snapshot associated with wrong VM"
pass "snapshot option on second VM"

if ! "$VMM" -v "$SCRIPT_DIR/configs/snapshot_vm1.conf" \
    -s "$SCRIPT_DIR/snapshots/snapshot_vm1.snapshot" \
    -v "$SCRIPT_DIR/configs/snapshot_vm2.conf" >examples.out 2>examples.err; then
    cat examples.err >&2
    fail "runnable mixed-VM examples"
fi
sed -n '1,32p' examples.out >example-fresh.out
sed -n '33,64p' examples.out >example-restored.out
assert_dump_format example-fresh.out "example fresh VM"
assert_register example-fresh.out 8 105 "example fresh VM starts from zero"
assert_register example-fresh.out 16 0 "example fresh VM has no restored registers"
cmp restored.out example-restored.out || fail "example VM restore differs"
[ "$(wc -l <examples.out | tr -d ' ')" = 64 ] || fail "wrong example output length"
pass "runnable examples restore VM1 and start VM2 fresh"

awk '{ printf "%s\r\n", $0 } END { printf " \t\r\n" }' resume.snapshot >crlf.snapshot
if ! "$VMM" -v snapshot.conf -s crlf.snapshot >crlf-restored.out 2>crlf-restored.err; then
    cat crlf-restored.err >&2
    fail "CRLF snapshot restore"
fi
cmp restored.out crlf-restored.out || fail "CRLF snapshot changed state"
pass "CRLF snapshot with trailing whitespace"

if ! "$VMM" -v snapshot.conf -s resume.snapshot -v snapshot.conf -s second.snapshot \
    >two-restored.out 2>two-restored.err; then
    cat two-restored.err >&2
    fail "two restored VMs"
fi
sed -n '1,32p' two-restored.out >first-restored.out
sed -n '33,64p' two-restored.out >second-restored.out
cmp restored.out first-restored.out || fail "first restored VM differs"
cmp restored.out second-restored.out || fail "second restored VM differs"
pass "independent snapshots for multiple VMs"

printf '%s\n' 'SNAPSHOT end.snapshot' >end.asm
printf '%s\n' 'vm_exec_slice_in_instructions=1' 'vm_binary=end.asm' >end.conf
run_success "snapshot at final instruction" "$TMP_DIR/end.conf" "$TMP_DIR/end.out"
mv end.snapshot finished.snapshot
mkdir end.snapshot
if ! "$VMM" -v end.conf -s finished.snapshot >finished.out 2>finished.err; then
    cat finished.err >&2
    fail "restore at program end"
fi
[ ! -s finished.out ] || fail "finished VM executed instructions"
pass "restore at program end halts normally"

run_args_failure "missing snapshot file" "Cannot read snapshot" -v snapshot.conf -s missing.snapshot
run_args_failure "snapshot write failure" "Cannot write snapshot" -v snapshot.conf
run_args_failure "snapshot failure allows other VMs to finish" "Cannot write snapshot" \
    -v snapshot.conf -v "$SCRIPT_DIR/configs/registers.conf"
sed -n '1,32p' "$TMP_DIR/failure.out" >failure-fresh.out
cmp "$REGISTER_OUTPUT" failure-fresh.out || fail "snapshot error prevented fresh VM from completing"
printf '%s\n' 'SNAPSHOT missing-directory/output.snapshot' >end.asm
run_args_failure "missing snapshot output directory" "Cannot write snapshot" -v end.conf
printf '%s\n' 'SNAPSHOT' >end.asm
run_args_failure "missing SNAPSHOT operand" "Invalid operands" -v end.conf
printf '%s\n' 'SNAPSHOT one two' >end.asm
run_args_failure "extra SNAPSHOT operand" "Invalid operands" -v end.conf

# Mutate one field at a time in an otherwise valid snapshot.
for mutation in '1:BAD_HEADER' '1:MIPS_VMM_SNAPSHOT 2' '2:1' '2:4000' \
    '3:-1' '4:4294967296' '5:1' '6:abc' '6:18446744073709551616' '6:12junk'; do
    field=${mutation%%:*}
    value=${mutation#*:}
    awk -v field="$field" -v value="$value" 'NR == field { $0 = value } { print }' \
        resume.snapshot >bad.snapshot
    run_args_failure "invalid snapshot field $mutation" "Invalid snapshot" -v snapshot.conf -s bad.snapshot
done
sed -n '1,35p' resume.snapshot >bad.snapshot
run_args_failure "truncated snapshot" "Invalid snapshot" -v snapshot.conf -s bad.snapshot
cp resume.snapshot bad.snapshot
printf '%s\n' 'extra data' >>bad.snapshot
run_args_failure "extra snapshot data" "Invalid snapshot" -v snapshot.conf -s bad.snapshot
printf '' >bad.snapshot
run_args_failure "empty snapshot" "Invalid snapshot" -v snapshot.conf -s bad.snapshot

run_args_failure "snapshot before VM" "Usage:" -s resume.snapshot -v snapshot.conf
run_args_failure "duplicate snapshot option" "Usage:" -v snapshot.conf -s resume.snapshot -s resume.snapshot
run_args_failure "missing snapshot argument" "Usage:" -v snapshot.conf -s
run_args_failure "missing VM argument" "Usage:" -v snapshot.conf -s resume.snapshot -v
run_args_failure "unknown option" "Usage:" -v snapshot.conf -x resume.snapshot

echo "All $TEST_COUNT tests passed."
