# Logical operations and shifts
addi $zero, $zero, 100
addiu $t0, $zero, -1
ori $t1, $zero, 0x00f0
andi $t2, $t0, 0x0ff0
or $t3, $t1, $t2
and $t4, $t1, $t2
sll $t5, $t1, 4
srl $t6, $t0, 28

DUMP_PROCESSOR_STATE
