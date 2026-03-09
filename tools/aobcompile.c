#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define AOB_MAGIC 0x00424F41
#define AOB_VERSION 1

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t entry_point;
    uint32_t code_size;
    uint32_t data_size;
    uint32_t bss_size;
    char name[32];
    uint32_t reserved[4];
} __attribute__((packed)) aob_header_t;

void print_usage(const char* prog) {
    printf("AOB Compiler - Aleksandar Ovcharov's Binary Format\n");
    printf("Usage: %s <input.bin> <output.aob> [program_name]\n", prog);
    printf("\n");
    printf("Arguments:\n");
    printf("  input.bin     - Raw binary machine code file\n");
    printf("  output.aob    - Output AOB executable file\n");
    printf("  program_name  - Optional program name (default: output filename)\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s hello.bin hello.aob \"Hello Program\"\n", prog);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char* input_file = argv[1];
    const char* output_file = argv[2];
    const char* program_name = (argc >= 4) ? argv[3] : output_file;
    
    FILE* input = fopen(input_file, "rb");
    if (!input) {
        fprintf(stderr, "Error: Cannot open input file '%s'\n", input_file);
        return 1;
    }
    
    fseek(input, 0, SEEK_END);
    long code_size = ftell(input);
    fseek(input, 0, SEEK_SET);
    
    if (code_size <= 0) {
        fprintf(stderr, "Error: Input file is empty\n");
        fclose(input);
        return 1;
    }
    
    if (code_size > 16384 - sizeof(aob_header_t)) {
        fprintf(stderr, "Error: Code size (%ld bytes) exceeds maximum (%lu bytes)\n", 
                code_size, 16384 - sizeof(aob_header_t));
        fclose(input);
        return 1;
    }
    
    uint8_t* code_buffer = (uint8_t*)malloc(code_size);
    if (!code_buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(input);
        return 1;
    }
    
    if (fread(code_buffer, 1, code_size, input) != (size_t)code_size) {
        fprintf(stderr, "Error: Failed to read input file\n");
        free(code_buffer);
        fclose(input);
        return 1;
    }
    fclose(input);
    
    aob_header_t header;
    memset(&header, 0, sizeof(header));
    
    header.magic = AOB_MAGIC;
    header.version = AOB_VERSION;
    header.flags = 0;
    header.entry_point = 0;
    header.code_size = code_size;
    header.data_size = 0;
    header.bss_size = 0;
    
    strncpy(header.name, program_name, sizeof(header.name) - 1);
    header.name[sizeof(header.name) - 1] = '\0';
    
    FILE* output = fopen(output_file, "wb");
    if (!output) {
        fprintf(stderr, "Error: Cannot create output file '%s'\n", output_file);
        free(code_buffer);
        return 1;
    }
    
    if (fwrite(&header, 1, sizeof(header), output) != sizeof(header)) {
        fprintf(stderr, "Error: Failed to write header\n");
        free(code_buffer);
        fclose(output);
        return 1;
    }
    
    if (fwrite(code_buffer, 1, code_size, output) != (size_t)code_size) {
        fprintf(stderr, "Error: Failed to write code\n");
        free(code_buffer);
        fclose(output);
        return 1;
    }
    
    fclose(output);
    free(code_buffer);
    
    printf("AOB file created successfully!\n");
    printf("  Input:  %s (%ld bytes)\n", input_file, code_size);
    printf("  Output: %s (%lu bytes)\n", output_file, sizeof(header) + code_size);
    printf("  Name:   %s\n", header.name);
    
    return 0;
}
