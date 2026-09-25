#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

/* ======================================== Global Constants ======================================== */
#define MIPS_REGISTER_COUNT 32
#define MAX_LINE_LENGTH 256


/* ======================================== Structs & Enums ========================================= */
// MIPS processor state structure
typedef struct {
    uint32_t registers[MIPS_REGISTER_COUNT];
    uint32_t hi;
    uint32_t lo;
    // Program Counter
    uint32_t pc;
} ProcessorState;

// Register alias structure for mapping MIPS register names to their numbers
typedef struct {
    const char *name;
    int number;
} RegisterNums;

// Register aliases for MIPS registers
static const RegisterNums register_nums[] = {
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
    OP_MFHI,
    OP_MFLO,
    // Logical operations
    OP_AND,
    OP_ANDI,
    OP_OR,
    OP_ORI,
    OP_SLL,
    OP_SRL,
    // Control flow operations
    OP_DUMP_PROCESSOR_STATE
} Opcode;

// Opcode alias structure for mapping MIPS opcode names to their corresponding enum values
typedef struct {
    const char *name;
    Opcode opcode;
} OpcodeAlias;

typedef enum {
    d_s_t,      // Destination, Source, Target format (e.g., add $v0, $a0, $a1)
    d_s_t_u,     // Destination, Source, Target unsigned format (e.g., addu $v0, $a0, $a1)
    t_s_i,      // Target, Source, Immediate format (e.g., addi $v0, $a0, 1)
    t_s_i_u,    // Target, Source, Immediate unsigned format (e.g., andi $v0, $a0, 1)
    s_t,        // Source, Target format (e.g., sub $v0, $a0)
    shift,      // Shift amount format (e.g., sll $v0, $a0, 2)
    d,          // Destination register format (e.g., mfhi $v0)
    none        // No operands format
} InstructionFormat;

typedef struct {
    const char *name;
    Opcode opcode;
    InstructionFormat format;
} InstructionTemplate;

// Opcode aliases for MIPS instructions
static const InstructionTemplate instruction_defs[] = {
    {"add",  OP_ADD, d_s_t},
    {"addi",  OP_ADDI, t_s_i},
    {"addu",  OP_ADDU, d_s_t_u},
    {"addiu", OP_ADDIU, t_s_i},
    {"sub",   OP_SUB,  d_s_t},
    {"subu",  OP_SUBU, d_s_t_u},
    {"mul",  OP_MUL,   d_s_t},
    {"mult" ,OP_MULT , s_t},
    {"div" ,OP_DIV , s_t},
    {"mfhi", OP_MFHI, d},
    {"mflo", OP_MFLO, d},

    {"and" ,OP_AND , d_s_t},
    {"andi" ,OP_ANDI , t_s_i_u},
    {"or" ,OP_OR , d_s_t},
    {"ori" ,OP_ORI , t_s_i_u},
    {"sll" ,OP_SLL , shift},
    {"srl" ,OP_SRL , shift},

    {"dump_processor_state", OP_DUMP_PROCESSOR_STATE, none}
};

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


/* ======================================== Helper Functions ======================================== */
// Trims leading and trailing whitespace from a string
static char *trim(char *text)
{
    char *end;

    while (isspace((unsigned char)*text)) {
        text++;
    }

    if (*text == '\0') {
        return text;
    }

    end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char)*end)) {
        end--;
    }
    end[1] = '\0';

    return text;
}

// Parses a 32-bit integer from a given string
static bool parse_int32(const char *text, int32_t *value)
{
    char *end = NULL;
    long long parsed;

    if (text == NULL || *text == '\0' || value == NULL) {
        return false;
    }

    errno = 0;
    parsed = strtoll(text, &end, 0);

    if (errno != 0 || end == text || *end != '\0' ||
        parsed < INT32_MIN || parsed > INT32_MAX) {
        return false;
    }

    *value = (int32_t)parsed;
    return true;
}

static bool parse_size(const char *text, size_t *value)
{
    char *end = NULL;
    unsigned long long parsed;

    if (text == NULL || *text == '\0' || value == NULL || *text == '-') {
        return false;
    }

    errno = 0;
    parsed = strtoull(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        parsed == 0 || parsed > SIZE_MAX) {
        return false;
    }

    *value = (size_t)parsed;
    return true;
}


/* ======================================== VM Functions ======================================== */

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
        printf("R%d=%" PRIu32 "\n", i, vm->cpu.registers[i]);
    }
}

static bool signed_add(int32_t a, int32_t b, int32_t *result)
{
    int64_t val = (int64_t)a + b;
    // Implement signed addition with overflow check
    if (val < INT32_MIN || val > INT32_MAX)
    {
        return false; // Overflow
    }

    *result = (int32_t)val;
    return true;
}

static bool signed_sub(int32_t a, int32_t b, int32_t *result)
{
    int64_t val = (int64_t)a - b;
    if (val < INT32_MIN || val > INT32_MAX)
    {
        return false; // Overflow
    }

    *result = (int32_t)val;
    return true;
}


/* ======================================== Execution Engine ======================================== */
// Executes a single instruction for the given VM and instruction
ExecStatus execute_engine(VirtualMachine *vm, const Instruction *instruction)
{
    ProcessorState *cpu = &vm->cpu;
    int32_t result;

    // Execute the instruction based on its opcode
    switch (instruction->opcode) {
        // Arithmetic ops
        case OP_ADD:
            if (!signed_add((int32_t)cpu->registers[instruction->rs],
                (int32_t)cpu->registers[instruction->rt], &result)) {
                fprintf(stderr, "Arithmetic overflow at source line %u\n", instruction->source_line);
                return EXEC_ERROR;
            }
            cpu->registers[instruction->rd] = (uint32_t)result;
            break;

        case OP_SUB:
            if (!signed_sub((int32_t)cpu->registers[instruction->rs],
                (int32_t)cpu->registers[instruction->rt], &result)) {
                fprintf(stderr, "Arithmetic overflow at source line %u\n", instruction->source_line);
                return EXEC_ERROR;
            }
            cpu->registers[instruction->rd] = (uint32_t)result;
            break;

        case OP_ADDI:
            if (!signed_add((int32_t)cpu->registers[instruction->rs],
                instruction->immediate, &result)) {
                fprintf(stderr, "Arithmetic overflow at source line %u\n", instruction->source_line);
                return EXEC_ERROR;
            }
            cpu->registers[instruction->rt] = (uint32_t)result;
            break;

        case OP_ADDU:
            cpu->registers[instruction->rd] = cpu->registers[instruction->rs] + cpu->registers[instruction->rt];
            break;

        case OP_ADDIU:
            cpu->registers[instruction->rt] = cpu->registers[instruction->rs] + (uint32_t)instruction->immediate;
            break;

        case OP_SUBU:
            cpu->registers[instruction->rd] = cpu->registers[instruction->rs] - cpu->registers[instruction->rt];
            break;

        case OP_MUL: {
            uint64_t product = (uint64_t)cpu->registers[instruction->rs] *
                (uint64_t)cpu->registers[instruction->rt];
            cpu->registers[instruction->rd] = (uint32_t)product;
            break;
        }

        case OP_MULT: {
            int64_t product = (int64_t)(int32_t)cpu->registers[instruction->rs] *
                (int64_t)(int32_t)cpu->registers[instruction->rt];
            uint64_t bits = (uint64_t)product;
            cpu->hi = (uint32_t)(bits >> 32);
            cpu->lo = (uint32_t)bits;
            break;
        }

        case OP_DIV: {
            int32_t dividend = (int32_t)cpu->registers[instruction->rs];
            int32_t divisor = (int32_t)cpu->registers[instruction->rt];

            if (divisor == 0) {
                fprintf(stderr, "Division by zero at source line %u\n", instruction->source_line);
                return EXEC_ERROR;
            }

            if (dividend == INT32_MIN && divisor == -1) {
                cpu->lo = (uint32_t)INT32_MIN;
                cpu->hi = 0;
            } else {
                cpu->lo = (uint32_t)(dividend / divisor);
                cpu->hi = (uint32_t)(dividend % divisor);
            }
            break;
        }

        case OP_MFHI:
            cpu->registers[instruction->rd] = cpu->hi;
            break;

        case OP_MFLO:
            cpu->registers[instruction->rd] = cpu->lo;
            break;
        
        // Conditional ops
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


/* ======================================== Instruction Parsing ======================================== */
// Instruction Parsing Helpers
static bool same_instruction_name(const char *left, const char *right)
{
    while (*left && *right) {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
            return false;
        }
        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}

// Finds the instruction template by name from the list of defined instruction templates. Returns NULL if not found.
static const InstructionTemplate *find_instruction_template(const char *name)
{
    size_t count = sizeof(instruction_defs) / sizeof(instruction_defs[0]);
    for (size_t i = 0; i < count; ++i) {
        if (same_instruction_name(instruction_defs[i].name, name)) {
            return &instruction_defs[i];
        }
    }

    return NULL;
}

// Tokenizes a line of text into individual tokens,
// Ignores comments and stores the tokens in the provided array.
static int tokenize_line(char *line, char *tokens[4])
{
    char *comment = strchr(line, '#');
    char *token;
    int token_count = 0;

    // If a comment is found, truncate the line at the comment character
    if (comment) {
        *comment = '\0';
    }

    for (char *p = line; *p; ++p) {
        if (*p == ',') {
            *p = ' ';
        }
    }

    // Tokenize the line using strtok_r
    token = strtok(line, " \t\n");
    while (token) {
        if (token_count == 4)
        {
            return -1;
        }

        tokens[token_count++] = token;
        token = strtok(NULL, " \t\r\n");
    }

    return token_count;
}

// Register string token -> Register number
static bool parse_register(const char *text, int *reg_num)
{
    const char *name;
    char *end = NULL;
    long num;

    if (!text || !reg_num)
    {
        return false;
    }

    // Skip leading whitespace
    while (isspace((unsigned char)*text))
    {
        ++text;
    }

    name = text;
    if (*name == '$')
    {
        name++;
    }

    if (*name == 'R' || *name == 'r')
    {
        name++;
    }

    // Check if the register is specified as a number
    if (isdigit((unsigned char)*name))
    {
        errno = 0;
        num = strtol(name, &end, 10);
        if (end == name || *end != '\0' || num < 0 || num > 31) {
            return false;
        }

        *reg_num = (int)num;
        return true;
    }

    // Check if the register is specified by name
    for (size_t i = 0; i < sizeof(register_nums) / sizeof(register_nums[0]); i++) {
        if (strcmp(name, register_nums[i].name) == 0) {
            *reg_num = (int)register_nums[i].number;
            return true;
        }
    }

    return false;
}

// Parse a single instruction from a line of assembly code
static int parse_instruction(char *line, unsigned line_num, const char *filename, Instruction *instr)
{
    char *tokens[4];
    int op_count;
    int32_t num;
    const InstructionTemplate *template;

    int token_count = tokenize_line(line, tokens);

    // No more instructions to parse
    if (token_count == 0) {
        return 0;
    }

    if (token_count < 0){
        fprintf(stderr, "Error: Tokenization failed in line %u of %s\n", line_num, filename);
        return -1;
    }

    if (token_count > 4)
    {
        fprintf(stderr, "Error: Too many tokens in line %u of %s\n", line_num, filename);
        return -1;
    }

    template = find_instruction_template(tokens[0]);

    if (!template) {
        fprintf(stderr, "Error: Unknown instruction '%s' in line %u of %s\n", tokens[0], line_num, filename);
        return -1;
    }

    // Initialize the instruction structure with default values
    *instr = (Instruction){
        .opcode = template->opcode,
        .rd = -1,
        .rs = -1,
        .rt = -1,
        .source_line = line_num
    };

    op_count = token_count - 1;

    // Parse the operands based on the instruction format
    switch (template->format) {
        // Format: rd, rs, rt
        case d_s_t:
        case d_s_t_u:
            if (op_count != 3 || !parse_register(tokens[1], &instr->rd)
                || !parse_register(tokens[2], &instr->rs)
                || !parse_register(tokens[3], &instr->rt)) {
                goto invalid_ops;
            }
            break;
        // Format: rt, rs, immediate
        case t_s_i:
            if (op_count != 3 || !parse_register(tokens[1], &instr->rt)
                || !parse_register(tokens[2], &instr->rs)
                || !parse_int32(tokens[3], &num)
                || num < INT16_MIN || num > INT16_MAX) {
                goto invalid_ops;
            }
            instr->immediate = num;
            break;
        case t_s_i_u:
            if (op_count != 3 || !parse_register(tokens[1], &instr->rt)
                || !parse_register(tokens[2], &instr->rs)
                || !parse_int32(tokens[3], &num)
                || num < 0 || num > UINT16_MAX) {
                goto invalid_ops;
            }
            instr->immediate = num;
            break;
        // Format: rs, rt
        case s_t:
            if (op_count != 2 || !parse_register(tokens[1], &instr->rs)
                || !parse_register(tokens[2], &instr->rt)) {
                goto invalid_ops;
            }
            break;
        // Format: rd, rt, shift amount
        case shift:
            if (op_count != 3 || !parse_register(tokens[1], &instr->rd)
                || !parse_register(tokens[2], &instr->rt)
                || !parse_int32(tokens[3], &num)
                || num < 0 || num > 31) {
                goto invalid_ops;
            }
            instr->shift_amount = (uint8_t)num;
            break;
        // Format: rd
        case d:
            if (op_count != 1 || !parse_register(tokens[1], &instr->rd)) {
                goto invalid_ops;
            }
            break;
        case none:
            if (op_count != 0) {
                goto invalid_ops;
            }
            break;
        default:
            fprintf(stderr, "Error: Unsupported instruction format for '%s' in line %u of %s\n", tokens[0], line_num, filename);
            return -1;
    }

    return 1;

invalid_ops:
    fprintf(stderr, "Error: Invalid operands in line %u of %s\n", line_num, filename);
    return -1;
}

// Check if a line is too long
static bool line_too_long(const char *line, FILE *file)
{
    return strchr(line, '\n') == NULL && !feof(file);
}

// Count the number of instructions in a file
static bool count_instructions(FILE *file, const char *filename, size_t *count)
{
    char line[MAX_LINE_LENGTH];
    unsigned line_num = 0;

    *count = 0;

    while (fgets(line, sizeof(line), file)) {
        char *comment;

        line_num++;
        if (line_too_long(line, file)) {
            fprintf(stderr, "Error: Line too long in %s at line %u\n", filename, line_num);
            return false;
        }

        comment = strchr(line, '#');
        if (comment) {
            *comment = '\0';
        }

        if (*trim(line))
        {
            (*count)++;
        }
    }

    if (ferror(file))
    {
        fprintf(stderr, "Error: Failed to read from %s\n", filename);
        return false;
    }
    return true;
}

// Load a program into the virtual machine
static bool load_program(VirtualMachine *vm, const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Failed to open %s\n", filename);
        return false;
    }

    Instruction *program = NULL;
    size_t instruction_count = 0;
    size_t loaded_count = 0;
    char line[MAX_LINE_LENGTH];
    unsigned line_num = 0;
    bool result = false;

    // Count the # of instructs in the file to determine the size of the program array
    if (!count_instructions(file, filename, &instruction_count) || instruction_count > UINT32_MAX
        || instruction_count > SIZE_MAX / sizeof(*program)) {
        goto finished_loading;
    }

    // Allocate memory for the program array based on the instruction count
    if (instruction_count) {
        program = malloc(instruction_count * sizeof(*program));
        if (!program) {
            fprintf(stderr, "Error: Failed to allocate memory for program\n");
            goto finished_loading;
        }
    }

    if (fseek(file, 0, SEEK_SET) != 0)
    {
        fprintf(stderr, "Error: Cannot seek to the beginning of %s\n", filename);
        goto finished_loading;
    }

    // Reset the file pointer to the beginning of the file before loading instructions
    while (fgets(line, sizeof(line), file)) {
        Instruction instruct;
        int res;

        line_num++;
        res = parse_instruction(line, line_num, filename, &instruct);

        if (res < 0)
        {
            goto finished_loading;
        }

        if (res == 1)
        {
            if (loaded_count == instruction_count) {
                fprintf(stderr, "Error: More instructions loaded than expected\n");
                goto finished_loading;
            }
            program[loaded_count++] = instruct;
        }
    }

    // Check for read errors after loading instructions
    if (ferror(file))
    {
        fprintf(stderr, "Error: Failed to read from %s\n", filename);
        goto finished_loading;
    }

    // Assign the loaded program to the virtual machine's instruction array
    vm->instructions = program;
    vm->instruction_count = loaded_count;
    program = NULL;
    result = true;

// Clean up and exit point for loading the program
finished_loading:
    free(program);
    fclose(file);
    return result;
}

// Parse and load the config file for a VM
static bool load_config(VirtualMachine *vm, unsigned id, const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Failed to open %s\n", filename);
        return false;
    }

    char line[MAX_LINE_LENGTH];
    char program_filepath[MAX_LINE_LENGTH] = "";
    unsigned line_num = 0;
    bool have_slice = false;
    bool have_program = false;
    bool valid = true;

    // Initialize the virtual machine structure with the given ID
    *vm = (VirtualMachine){.id = id};

    // Parse the configuration file line by line
    while (valid && fgets(line, sizeof(line), file)) {
        char *comment = strchr(line, '#');
        char *equals;
        char *key;
        char *value;

        line_num++;
        if (line_too_long(line, file))
        {
            fprintf(stderr, "Error: Line %u in %s is too long\n", line_num, filename);
            valid = false;
            break;
        }

        // Allow comments in the configuration file
        if (comment)
        {
            *comment = '\0';
        }

        // Trim leading and trailing whitespace from the line to get the key-value pair
        key = trim(line);

        // Skip empty lines after trimming whitespace
        if (!*key)
        {
            continue;
        }

        // Find the '=' sign to separate the key and value
        equals = strchr(key, '=');
        if (!equals || strchr(equals + 1, '='))
        {
            fprintf(stderr, "Error: Line %u in %s has invalid key-value pair\n", line_num, filename);
            valid = false;
            break;
        }

        // Split the key and value at the '=' sign
        *equals = '\0'; 
        value = trim(equals + 1);
        key = trim(key);

        // Process the key-value pair based on the key
        if (strcmp(key, "vm_exec_slice_in_instructions") == 0)
        {
            if (have_slice || !parse_size(value, &vm->execution_slice))
            {
                fprintf(stderr, "Error: Line %u in %s has invalid value for vm_exec_slice_in_instructions\n", line_num, filename);
                valid = false;
            }
            else
            {
                have_slice = true;
            }
        }

        // Handle the vm_binary key-value pair
        else if (strcmp(key, "vm_binary") == 0)
        {
            if (have_program || *value == '\0' || strlen(value) >= sizeof(program_filepath))
            {
                fprintf(stderr, "Error: Line %u in %s has invalid value for vm_binary\n", line_num, filename);
                valid = false;
            }
            else
            {
                have_program = true;
                strcpy(program_filepath, value);
            }
        }

        // Handle any unknown keys
        else
        {
            fprintf(stderr, "Error: Line %u in %s has unknown key '%s'\n", line_num, filename, key);
            valid = false;
        }
    }

    if (ferror(file))
    {
        valid = false;
    }

    fclose(file);

    if (!valid || !have_program || !have_slice)
    {
        if (valid)
        {
            fprintf(stderr, "Error: Failed to parse configuration file %s\n", filename);
        }
        return false;
    }

    // Load the program into the virtual machine using the parsed configuration
    return load_program(vm, program_filepath);
}

// Run multiple VMs concurrently
static bool run_vms(VirtualMachine *vms, size_t count)
{
    size_t active = count;
    bool success = true;

    while (active)
    {
        // Continue running VMs until all are halted or have errored out
        for (size_t i = 0; i < count; i++)
        {
            VirtualMachine *vm = &vms[i];

            if (vm->status == VM_HALTED || vm->status == VM_ERROR)
            {
                continue;
            }

            vm->status = VM_RUNNING;

            // Execute a slice of instructions for the current VM
            for (size_t step = 0; step < vm->execution_slice; step++)
            {
                ExecStatus result = execute_instruction(vm);
                if (result == EXEC_FINISHED)
                {
                    vm->status = VM_HALTED;
                    active--;
                    break;
                }

                if (result == EXEC_ERROR)
                {
                    vm->status = VM_ERROR;
                    active--;
                    success = false;
                    break;
                }
            }
        }
    }

    return success;
}

// Print usage information for the command-line tool
static void usage(const char *name)
{
    fprintf(stderr, "Usage: %s -v config_file [-v config_file ...] \n", name);
}

// Main function to run VMs
int main(int argc, char **argv)
{
    // Allocate memory for the VMs and initialize variables
    VirtualMachine *vms;
    size_t vm_count;
    int status = EXIT_FAILURE;

    if (argc < 3 || (argc - 1) % 2)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    vm_count = (size_t)(argc - 1) / 2;
    vms = calloc(vm_count, sizeof(*vms));
    if (!vms)
    {
        fprintf(stderr, "Failed to allocate memory for VMs.\n");
        return EXIT_FAILURE;
    }

    // Parse and load the config file for each VM
    for (size_t i = 0; i < vm_count; i++)
    {
        size_t arg = 1 + i * 2;
        if (strcmp(argv[arg], "-v") != 0 || !load_config(&vms[i], (unsigned)i + 1, argv[arg + 1]))
        {
            if (strcmp(argv[arg], "-v") != 0)
            {
                usage(argv[0]);
            }
            goto done;
        }
    }

    // Run all the VMs
    status = run_vms(vms, vm_count) ? EXIT_SUCCESS : EXIT_FAILURE;

// Clean up and free allocated memory for the VMs
done:
    for (size_t i = 0; i < vm_count; i++)
    {
        free(vms[i].instructions);
    }
    free(vms);
    return status;
}
