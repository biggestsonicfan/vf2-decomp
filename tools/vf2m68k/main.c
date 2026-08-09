#include "vf2/m68k.h"
#include "vf2/rom.h"
#include "vf2/status.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *program)
{
    fprintf(stderr, "Usage:\n  %s info <rom-directory>\n  %s disasm <rom-directory> <address> <count>\n",
            program, program);
}

static int parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed = 0ul;
    errno = 0;
    parsed = strtoul(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT32_MAX) {
        return 0;
    }
    *value = (uint32_t)parsed;
    return 1;
}

static int build_audio(const char *directory, uint8_t **program, size_t *size)
{
    vf2_status status = vf2_romset_build_region(
        directory, VF2_REGION_AUDIOCPU, program, size
    );
    if (status != VF2_OK) {
        fprintf(stderr, "Failed to build audiocpu: %s\n",
                vf2_status_string(status));
        return 0;
    }
    return 1;
}

static int command_info(const char *directory)
{
    uint8_t *program = NULL;
    size_t size = 0u;
    uint32_t initial_stack = 0u;
    uint32_t initial_pc = 0u;
    uint32_t vector = 0u;
    size_t index = 0u;
    if (!build_audio(directory, &program, &size)) {
        return EXIT_FAILURE;
    }
    if (vf2_m68k_read_u32(program, size, 0u, &initial_stack) != VF2_OK ||
        vf2_m68k_read_u32(program, size, 4u, &initial_pc) != VF2_OK) {
        free(program);
        return EXIT_FAILURE;
    }
    printf("Audio CPU region: 0x%zx bytes\n", size);
    printf("Initial stack:    0x%08x\n", (unsigned)initial_stack);
    printf("Initial PC:       0x%08x\n", (unsigned)initial_pc);
    printf("Exception vectors:\n");
    for (index = 0u; index < 16u; ++index) {
        if (vf2_m68k_read_u32(program, size, (uint32_t)index * 4u, &vector) != VF2_OK) {
            free(program);
            return EXIT_FAILURE;
        }
        printf("  [%02zu] 0x%08x\n", index, (unsigned)vector);
    }
    free(program);
    return EXIT_SUCCESS;
}

static int command_disasm(const char *directory, const char *address_text,
                          const char *count_text)
{
    uint8_t *program = NULL;
    size_t size = 0u;
    uint32_t address = 0u;
    uint32_t count = 0u;
    uint32_t index = 0u;
    if (!parse_u32(address_text, &address) ||
        !parse_u32(count_text, &count) || count == 0u) {
        fprintf(stderr, "Invalid address or count.\n");
        return EXIT_FAILURE;
    }
    if (!build_audio(directory, &program, &size)) {
        return EXIT_FAILURE;
    }
    for (index = 0u; index < count; ++index) {
        vf2_m68k_instruction instruction;
        vf2_status status = vf2_m68k_decode(program, size, address,
                                             &instruction);
        if (status != VF2_OK) {
            fprintf(stderr, "Decode failed at 0x%08x: %s\n", (unsigned)address,
                    vf2_status_string(status));
            free(program);
            return EXIT_FAILURE;
        }
        printf("0x%08x: %-24s%s\n", (unsigned)address, instruction.text,
               instruction.supported ? "" : "  [unsupported opcode]");
        address += (uint32_t)instruction.length;
    }
    free(program);
    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (strcmp(argv[1], "info") == 0 && argc == 3) {
        return command_info(argv[2]);
    }
    if (strcmp(argv[1], "disasm") == 0 && argc == 5) {
        return command_disasm(argv[2], argv[3], argv[4]);
    }
    usage(argv[0]);
    return EXIT_FAILURE;
}
