# A fresh VM must have zero in $t0, even when VM1 restores a snapshot.
addi $t0, $t0, 100
addi $t0, $t0, 5
DUMP_PROCESSOR_STATE
