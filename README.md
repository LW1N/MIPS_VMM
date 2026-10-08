# MIPS Virtual Machine Monitor

A small MIPS interpreter that runs one or more virtual CPUs with configurable
instruction time slices.

## Build

On Debian or Ubuntu, install the required tools:

```sh
sudo apt update
sudo apt install build-essential
```

Then build and test:

```sh
make test
```

## Run

Pass each VM configuration with `-v`:

```sh
./myvmm -v test/arithmetic.conf
./myvmm -v input_files/assembly_file_vm1 \
        -v input_files/assembly_file_vm2
```

A configuration file contains:

```ini
vm_exec_slice_in_instructions=2
vm_binary=instructions_vm1
```

Relative program paths are resolved from the configuration file's directory.

## Snapshots

Add `SNAPSHOT <filename>` to an assembly program to save its CPU state and
continue execution. Instruction names are case-insensitive. Snapshot output
paths are relative to the VMM's working directory; filenames cannot contain
whitespace, commas, or `#`. Existing output files are overwritten.

Use `-s` immediately after a VM's `-v config_file` to resume that VM:

```sh
./myvmm -v input_files/assembly_file_vm1 -s snapshot_vm1 \
        -v input_files/assembly_file_vm2
```

VM1 resumes at the instruction after `SNAPSHOT`; VM2 starts from the beginning.
Each VM may have its own `-s` option. Use the same assembly program when restoring:
snapshots do not include or verify the program. The execution slice comes from
the configuration file.

Snapshots are text files containing `MIPS_VMM_SNAPSHOT 1`, followed by the next
PC, HI, LO, and R0 through R31 as unsigned decimal values, one per line. Invalid
or unreadable snapshots fail startup. A failed snapshot write errors that VM;
other VMs continue running and the VMM exits with failure.

### Runnable examples

Run these commands from the repository root after `make`. The example configs,
assembly programs, and a ready-to-load snapshot are checked in under `tests/`.

Start VM1 from zero, create `checkpoint.snapshot` and `second.snapshot` in the
current directory, and continue to the end:

```sh
./myvmm -v tests/configs/snapshot_vm1.conf
```

This prints two register dumps. The first has R8=4294967289 (-7), R16=272,
and R17=289. The final dump has R8=4294967290 (-6), R16=4294967295 (HI),
and R17=4294967275 (LO).

Resume using the snapshot just created, or use the included snapshot directly:

```sh
./myvmm -v tests/configs/snapshot_vm1.conf -s checkpoint.snapshot
./myvmm -v tests/configs/snapshot_vm1.conf -s tests/snapshots/snapshot_vm1.snapshot
```

Both commands print only the final dump with the same values as the fresh run.
They start after the first `SNAPSHOT` and write `second.snapshot`.

Run restored VM1 alongside fresh VM2:

```sh
./myvmm -v tests/configs/snapshot_vm1.conf -s tests/snapshots/snapshot_vm1.snapshot \
        -v tests/configs/snapshot_vm2.conf
```

VM2 finishes first with R8=105 and all other registers zero. VM1 then prints
its final dump. To restore nearer the end, use `-s second.snapshot`: only the
final dump remains to execute, and neither snapshot instruction runs again.

To remove build output:

```sh
make clean
```
