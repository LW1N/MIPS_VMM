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
To remove build output:

```sh
make clean
```
