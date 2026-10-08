# Give every writable register a distinct value to check complete snapshots.
li $1, 17
li $2, 34
li $3, 51
li $4, 68
li $5, 85
li $6, 102
li $7, 119
li $8, 136
li $9, 153
li $10, 170
li $11, 187
li $12, 204
li $13, 221
li $14, 238
li $15, 255
li $16, 272
li $17, 289
li $18, 306
li $19, 323
li $20, 340
li $21, 357
li $22, 374
li $23, 391
li $24, 408
li $25, 425
li $26, 442
li $27, 459
li $28, 476
li $29, 493
li $30, 510
li $31, 527

# Set HI and LO to the upper and lower halves of -21.
li $t0, -7
li $t1, 3
mult $t0, $t1
DUMP_PROCESSOR_STATE
SNAPSHOT checkpoint.snapshot

# A restore starts here, preserving the earlier CPU state.
mfhi $s0
mflo $s1
addiu $t0, $t0, 1
snapshot second.snapshot
DUMP_PROCESSOR_STATE
