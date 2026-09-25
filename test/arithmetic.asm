# Signed and unsigned arithmetic
addi $t0, $zero, 12
addi $t1, $zero, -5
add $t2, $t0, $t1
addu $t3, $t0, $t1
addiu $t4, $t1, 20
sub $t5, $t0, $t1
subu $t6, $zero, $t0
mul $t7, $t0, $t1

# MULT and DIV write HI and LO
mult $t0, $t1
mfhi $s0
mflo $s1
div $t0, $t1
mfhi $s2
mflo $s3

DUMP_PROCESSOR_STATE
