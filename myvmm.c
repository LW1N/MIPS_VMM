#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

/* Global Constants */
#define MIPS_REGISTER_COUNT 32


/* Structs & Enums */
// MIPS processor state structure
typedef struct {
    uint32_t registers[MIPS_REGISTER_COUNT];
    // Program Counter
    uint32_t pc;
} ProcessorState;

// Register alias structure for mapping MIPS register names to their numbers
typedef struct {
    const char *name;
    int number;
} RegisterAlias;

// Register aliases for MIPS registers
static const RegisterAlias aliases[] = {
    // r0, Always 0
    {"zero", 0},
    // r1, Reserved for assembler
    {"at",   1},
    // r2-r3, Stores results
    {"v0",   2},
    {"v1",   3},
    // r4-r7, Stores arguments
    {"a0",   4},
    {"a1",   5},
    {"a2",   6},
    {"a3",   7},
    // r8-r15, Temp registers
    {"t0",   8},
    {"t1",   9},
    {"t2",  10},
    {"t3",  11},
    {"t4",  12},
    {"t5",  13},
    {"t6",  14},
    {"t7",  15},
    // r16-r23, Saved for later registers
    {"s0",  16},
    {"s1",  17},
    {"s2",  18},
    {"s3",  19},
    {"s4",  20},
    {"s5",  21},
    {"s6",  22},
    {"s7",  23},
    // r24-r25, More temp registers
    {"t8",  24},
    {"t9",  25},
    // r26-r27, Reserved for kernel
    {"k0",  26},
    {"k1",  27},
    // r28, Global pointer
    {"gp",  28},
    // r29, Stack pointer
    {"sp",  29},
    // r30, Frame pointer
    {"fp",  30},
    // r31, Return address
    {"ra",  31}
};

// Opcode enum for MIPS instructions
typedef enum {
    // Arithmetic operations
    OP_ADD,
    OP_ADDI,
    OP_ADDU,
    OP_ADDIU,
    OP_SUB,
    OP_SUBU,
    OP_MUL,
    OP_MULT,
    OP_DIV,
    // Logical operations
    OP_AND,
    OP_ANDI,
    OP_OR,
    OP_ORI,
    OP_SLL,
    OP_SRL,
    // Comparison operations
    OP_MFHI,
    OP_MFLO,
    // Control flow operations
    OP_DUMP_PROCESSOR_STATE
} Opcode;

// Parsed instruction struct
// Ex: "add $v0, $a0, $a1"
// "opcode $rd, $rs, $rt"
typedef struct {
    // Opcode of the instruction
    Opcode opcode;

    // Destination register
    int rd;
    // Source register
    int rs;
    // Target register
    int rt;

    // Immediate(Constant) value for the instruction
    // Used for instructions like addi, ori, etc.
    int32_t immediate;
    // Shift amount for shift instructions
    uint8_t shift_amount;

    // Source line number for debugging purposes
    unsigned source_line;
} Instruction;

// Execution status enum for instruction execution results
typedef enum {
    EXEC_OK,
    EXEC_FINISHED,
    EXEC_ERROR
} ExecStatus;

// Virtual machine status enum
typedef enum {
    VM_READY,
    VM_RUNNING,
    VM_HALTED,
    VM_ERROR
} VmStatus;

// Virtual machine structure
typedef struct {
    // Unique identifier for the virtual machine
    unsigned id;
    // Configuration file path for the virtual machine
    char *config_path;
    // Binary file path for the virtual machine
    char *binary_path;

    // Execution slice for the virtual machine
    size_t execution_slice;

    // CPU state for the virtual machine
    ProcessorState cpu;
    // Array of instructions for the virtual machine
    Instruction *instructions;
    // Number of instructions in the array
    size_t instruction_count;

    VmStatus status;
} VirtualMachine;


/* Functions */

// Initializes a virtual machine structure to its default state
void vm_init(VirtualMachine *vm, unsigned id)
{
    vm->id = id;
    vm->config_path = NULL;
    vm->binary_path = NULL;
    vm->execution_slice = 0;
    vm->cpu = (ProcessorState){0};
    vm->instructions = NULL;
    vm->instruction_count = 0;
    vm->status = VM_READY;
}

// Dumps the current state of the virtual machine's processor registers
void dump_processor_state(const VirtualMachine *vm)
{
    for (int i = 0; i < 32; i++) {
        printf("R%d=%d\n", i, vm->cpu.registers[i]);
    }
}

// Executes a single instruction for the given VM and instruction
ExecStatus execute_engine(VirtualMachine *vm, const Instruction *instruction)
{
    ProcessorState *cpu = &vm->cpu;

    switch (instruction->opcode) {
        case OP_ADD:
            cpu->registers[instruction->rd] = cpu->registers[instruction->rs] + cpu->registers[instruction->rt];
            break;

        case OP_SUB:
            cpu->registers[instruction->rd] = cpu->registers[instruction->rs] - cpu->registers[instruction->rt];
            break;

        case OP_ADDI:
            cpu->registers[instruction->rt] = cpu->registers[instruction->rs] + (uint32_t)instruction->immediate;
            break;

        case OP_AND:
            cpu->registers[instruction->rd] = cpu->registers[instruction->rs] & cpu->registers[instruction->rt];
            break;

        case OP_ANDI:
            cpu->registers[instruction->rt] = cpu->registers[instruction->rs] & (uint16_t)instruction->immediate;
            break;

        case OP_OR:
            cpu->registers[instruction->rd] = cpu->registers[instruction->rs] | cpu->registers[instruction->rt];
            break;

        case OP_ORI:
            cpu->registers[instruction->rt] = cpu->registers[instruction->rs] | (uint16_t)instruction->immediate;
            break;

        case OP_SLL:
            cpu->registers[instruction->rd] = cpu->registers[instruction->rt] << instruction->shift_amount;
            break;

        case OP_SRL:
            cpu->registers[instruction->rd] = cpu->registers[instruction->rt] >> instruction->shift_amount;
            break;

        case OP_DUMP_PROCESSOR_STATE:
            dump_processor_state(vm);
            break;
        default:
            // Handle unsupported instructions or errors here. 
            // For now, we'll just print a message. 
            fprintf(stderr, "Unsupported instruction: %d\n", instruction->opcode);
            return EXEC_ERROR;
    }

    // Ensure register 0 is always zero
    cpu->registers[0] = 0;

    // Return the execution status
    return EXEC_OK;
}

// Fetches VM instruction, decodes it, and executes it using the execution engine
ExecStatus execute_instruction(VirtualMachine *vm)
{
    ProcessorState *cpu = &vm->cpu;
    
    if (cpu->pc % 4 != 0) {
        fprintf(stderr, "VM: %d ERROR: Program counter is not aligned to 4 bytes: %d\n", vm->id, cpu->pc);
        return EXEC_ERROR;
    }

    // Calculate the instruction index based on the program counter
    size_t instruct_ind = cpu->pc / 4;

    // Check if the instruction index is within the valid range
    // If not, something is wrong with the program counter or instruction count
    if (instruct_ind > vm->instruction_count) {
        fprintf(stderr, "VM: %d ERROR: Instruction index out of bounds: %zu\n", vm->id, instruct_ind);
        return EXEC_ERROR;
    }

    // There are no more instructions to execute
    if (instruct_ind == vm->instruction_count) {
        return EXEC_FINISHED;
    }

    // Fetch the instruct and execute
    const Instruction *instr = &vm->instructions[instruct_ind];
    ExecStatus status = execute_engine(vm, instr);
    if (status == EXEC_OK) {
        cpu->pc += 4;
    }

    return status;
}

// Main function to run VMs
int main(void)
{
    // TESTING
    VirtualMachine vm;
    vm_init(&vm, 1);

    Instruction program[] = {
        {
            .opcode = OP_ADDI,
            .rs = 0,
            .rt = 8,
            .immediate = 12
        },
        {
            .opcode = OP_ADDI,
            .rs = 0,
            .rt = 9,
            .immediate = 10
        },
        {
            .opcode = OP_ADD,
            .rd = 10,
            .rs = 8,
            .rt = 9
        },
        {
            .opcode = OP_AND,
            .rd = 11,
            .rs = 8,
            .rt = 9
        },
        {
            .opcode = OP_DUMP_PROCESSOR_STATE
        }
    };

    Instruction zero_test[] = {
    {
        .opcode = OP_ADDI,
        .rs = 0,
        .rt = 0,
        .immediate = 100
    },
    {
        .opcode = OP_DUMP_PROCESSOR_STATE
    }
};

    vm.instructions = zero_test;
    vm.instruction_count = (sizeof(zero_test) / sizeof(zero_test[0]));

    while (execute_instruction(&vm) == EXEC_OK) {
        /* Continue until finished or error. */
    }

    return vm.status;
}