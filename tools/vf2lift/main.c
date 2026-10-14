/* vf2lift -- build-time i960->C recompiler (plan phase 4, Option B).
 *
 * Reads the user's own ROM, walks the control-flow graph with the existing
 * analysis engine, and emits one C function per reachable guest function into
 * the build directory. Generated code is NEVER committed (docs/VF2LIFT.md); it
 * is the same position as the interpreter, which also reads the ROM at run
 * time. Each lifted body is a native switch over the function's basic blocks
 * (compiled control flow) and runs every block through the shared interpreter
 * semantics via vf2_lift_run_block, so there is no second implementation of the
 * opcode semantics to drift from src/i960/executor.c.
 *
 * Usage:
 *   vf2lift <rom-dir> <out.c> <out.h> [--backlog <csv>] [entry-addr ...]
 *
 * Function selection:
 *   - with neither --backlog nor entry addresses, every function the CFG
 *     discovers (from the boot vectors and the scheduler task catalog) is
 *     emitted;
 *   - entry addresses select exactly the functions at those addresses;
 *   - --backlog <csv> reads a coverage backlog (the committed
 *     decomp/i960/coverage_attract_backlog.csv shape: a header then
 *     `start,end,words,hits` rows) and selects the function that contains each
 *     executed run start -- i.e. the functions the Phase 3 session actually
 *     reached. --backlog and entry addresses combine (union).
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/analysis/cfg.h"
#include "vf2/analysis/tasks.h"
#include "vf2/boot.h"
#include "vf2/i960/decoder.h"
#include "vf2/rom.h"
#include "vf2/status.h"
#include "vf2/version.h"

enum { VF2LIFT_MAX_ENTRIES = 512u };

static int parse_u32(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed = 0ul;

    errno = 0;
    parsed = strtoul(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || parsed > 0xfffffffful) {
        return 0;
    }
    *value = (uint32_t)parsed;
    return 1;
}

/* Append addr to list[*count] if absent and room remains. */
static void add_unique(uint32_t *list, size_t *count, size_t capacity, uint32_t addr)
{
    size_t i = 0u;

    for (i = 0u; i < *count; ++i) {
        if (list[i] == addr) {
            return;
        }
    }
    if (*count < capacity) {
        list[(*count)++] = addr;
    }
}

/* Fill entries[] with the boot vectors plus every scheduler task entry point
 * (the same discovery vf2i960's analysis uses), so find_function resolves the
 * real function boundaries rather than synthetic ones. Returns the count. */
static size_t discover_entries(
    const uint8_t *image,
    size_t image_size,
    const vf2_i960_boot_vectors *vectors,
    uint32_t *entries,
    size_t capacity
)
{
    vf2_task_catalog catalog;
    size_t count = 0u;
    size_t i = 0u;

    add_unique(entries, &count, capacity, vectors->start_ip);
    add_unique(entries, &count, capacity, 0x000001b0u);
    vf2_task_catalog_init(&catalog);
    if (vf2_task_catalog_scan(&catalog, image, image_size) == VF2_OK) {
        for (i = 0u; i < catalog.count; ++i) {
            add_unique(entries, &count, capacity, catalog.tasks[i].entry_point);
        }
    }
    vf2_task_catalog_destroy(&catalog);
    return count;
}

/* Read the run-start column of a coverage backlog CSV into starts[]. */
static size_t read_backlog_starts(
    const char *path,
    uint32_t *starts,
    size_t capacity
)
{
    FILE *csv = fopen(path, "r");
    char line[256];
    int header = 1;
    size_t count = 0u;

    if (csv == NULL) {
        fprintf(stderr, "vf2lift: cannot open backlog %s\n", path);
        return 0u;
    }
    while (fgets(line, sizeof(line), csv) != NULL) {
        uint32_t start = 0u;
        char *comma = strchr(line, ',');
        if (header) { header = 0; continue; } /* skip the header row */
        if (comma != NULL) { *comma = '\0'; }
        if (parse_u32(line, &start)) {
            add_unique(starts, &count, capacity, start);
        }
    }
    fclose(csv);
    return count;
}

/* Map each backlog run start to the function that contains it (the run starts
 * are seeded as analysis entries first, so they resolve), appending unique
 * function entries to requested[]. */
static void resolve_backlog_functions(
    const uint32_t *starts,
    size_t start_count,
    const vf2_i960_analysis *analysis,
    uint32_t *requested,
    size_t *requested_count,
    size_t capacity
)
{
    size_t i = 0u;
    size_t resolved = 0u;
    size_t unresolved = 0u;

    for (i = 0u; i < start_count; ++i) {
        const vf2_function *function =
            vf2_i960_find_function(analysis, starts[i]);
        if (function == NULL) {
            ++unresolved;
            continue;
        }
        add_unique(requested, requested_count, capacity, function->address);
        ++resolved;
    }
    fprintf(stderr,
            "vf2lift: backlog -> %zu/%zu run(s) resolved to %zu function(s)"
            " (%zu unresolved)\n",
            resolved, start_count, *requested_count, unresolved);
}

static void emit_banner(FILE *out, const char *rom_dir)
{
    fprintf(out,
        "/* GENERATED by vf2lift %s from the ROM at \"%s\".\n"
        " * Do NOT commit this file. It is rebuilt from the user's ROM at\n"
        " * build time (docs/VF2LIFT.md). Edits will be overwritten. */\n\n",
        VF2_VERSION_STRING, rom_dir);
}

/* Emit one lifted C body for `function`. Returns 1 if a function was written,
 * 0 if it was skipped (with a comment explaining why). */
/* A function is liftable if it has at least one basic block that starts inside
 * its [address, end) range. (Backlog seeding can create a function whose CFG
 * walk pulled in blocks below its entry; those out-of-range blocks are simply
 * not emitted -- the range-guarded loop below never reaches them.) */
static int function_is_liftable(
    const vf2_i960_analysis *analysis,
    const vf2_function *function
)
{
    size_t block_index = 0u;

    if (function->block_count == 0u || function->end <= function->address) {
        return 0;
    }
    for (block_index = function->first_block;
         block_index < function->first_block + function->block_count;
         ++block_index) {
        const vf2_basic_block *block = &analysis->blocks[block_index];
        if (block->end > block->start &&
            block->start >= function->address &&
            block->start < function->end) {
            return 1;
        }
    }
    return 0;
}

/* Emit one lifted C body for `function`. Returns 1 if a function was written,
 * 0 if it was skipped (with a comment explaining why).
 *
 * The body is a range-guarded loop over the function's basic blocks: while
 * cpu->ip is inside [address, end) it runs the block that starts there and
 * re-dispatches natively; the moment control leaves the range (a return, a
 * call, a tail branch or an indirect transfer) the loop ends and the driver
 * re-dispatches. The range guard makes the lifted stop condition identical to
 * the interpreter reference used by the differential, so the two cannot
 * diverge on anything but a genuine gap in the recovered CFG -- an address
 * inside the range that is not a block start -- on which the body fails closed
 * (VF2_ERROR_UNSUPPORTED) rather than guessing. */
static int emit_function(
    FILE *out,
    const vf2_i960_analysis *analysis,
    const vf2_function *function
)
{
    size_t block_index = 0u;
    size_t emitted_blocks = 0u;

    if (!function_is_liftable(analysis, function)) {
        fprintf(out, "/* skip %s @0x%08x: no in-range blocks */\n\n",
                function->name, (unsigned)function->address);
        return 0;
    }

    fprintf(out,
        "/* %s  blocks=%zu%s */\n"
        "vf2_status vf2_lifted_%08x(vf2_model2a *machine, vf2_i960_cpu *cpu)\n"
        "{\n"
        "    while (cpu->ip >= UINT32_C(0x%08x) && cpu->ip < UINT32_C(0x%08x)) {\n"
        "        switch (cpu->ip) {\n",
        function->name, function->block_count,
        function->has_indirect_flow ? " indirect" : "",
        (unsigned)function->address,
        (unsigned)function->address, (unsigned)function->end);

    for (block_index = function->first_block;
         block_index < function->first_block + function->block_count;
         ++block_index) {
        const vf2_basic_block *block = &analysis->blocks[block_index];
        if (block->end <= block->start ||
            block->start < function->address ||
            block->start >= function->end) {
            continue; /* out-of-range: unreachable under the range guard */
        }
        fprintf(out,
            "        case UINT32_C(0x%08x): {\n"
            "            const vf2_status s = vf2_lift_run_block(\n"
            "                machine, cpu, UINT32_C(0x%08x), UINT32_C(0x%08x));\n"
            "            if (s != VF2_OK) { return s; }\n"
            "            break;\n"
            "        }\n",
            (unsigned)block->start, (unsigned)block->start, (unsigned)block->end);
        ++emitted_blocks;
    }

    fprintf(out,
        "        default:\n"
        "            /* inside [0x%08x,0x%08x) but not a recovered block start:\n"
        "             * a gap in the CFG. Fail closed. */\n"
        "            return VF2_ERROR_UNSUPPORTED;\n"
        "        }\n"
        "    }\n"
        "    return VF2_OK; /* control left the function's range */\n"
        "}\n\n",
        (unsigned)function->address, (unsigned)function->end);
    (void)emitted_blocks;
    return 1;
}

/* Last path component of a '/'- or '\\'-separated path. */
static const char *path_basename(const char *path)
{
    const char *base = path;
    const char *p = path;

    for (; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

static void emit_escaped(FILE *out, const char *text)
{
    for (; *text != '\0'; ++text) {
        if (*text == '"' || *text == '\\') {
            fputc('\\', out);
        }
        fputc(*text, out);
    }
}

int main(int argc, char **argv)
{
    const char *rom_dir = NULL;
    const char *out_c_path = NULL;
    const char *out_h_path = NULL;
    uint32_t requested[VF2LIFT_MAX_ENTRIES];
    size_t requested_count = 0u;
    uint32_t entries[VF2LIFT_MAX_ENTRIES];
    size_t entry_count = 0u;
    uint32_t backlog_starts[VF2LIFT_MAX_ENTRIES];
    size_t backlog_start_count = 0u;
    uint8_t *image = NULL;
    size_t image_size = 0u;
    vf2_i960_boot_vectors vectors;
    vf2_i960_analysis analysis;
    vf2_status status = VF2_OK;
    FILE *out_c = NULL;
    FILE *out_h = NULL;
    size_t emitted = 0u;
    size_t index = 0u;
    int argi = 0;

    const char *backlog_path = NULL;

    if (argc < 4) {
        fprintf(stderr,
            "usage: %s <rom-dir> <out.c> <out.h> [--backlog <csv>] "
            "[entry-addr ...]\n", argv[0]);
        return EXIT_FAILURE;
    }
    rom_dir = argv[1];
    out_c_path = argv[2];
    out_h_path = argv[3];
    for (argi = 4; argi < argc; ++argi) {
        if (strcmp(argv[argi], "--backlog") == 0) {
            if (argi + 1 >= argc) {
                fprintf(stderr, "--backlog needs a path\n");
                return EXIT_FAILURE;
            }
            backlog_path = argv[++argi];
            continue;
        }
        if (requested_count >= VF2LIFT_MAX_ENTRIES) {
            fprintf(stderr, "too many entry addresses (max %u)\n",
                    (unsigned)VF2LIFT_MAX_ENTRIES);
            return EXIT_FAILURE;
        }
        if (!parse_u32(argv[argi], &requested[requested_count])) {
            fprintf(stderr, "bad entry address: %s\n", argv[argi]);
            return EXIT_FAILURE;
        }
        ++requested_count;
    }

    status = vf2_romset_build_region(rom_dir, VF2_REGION_MAINCPU,
                                     &image, &image_size);
    if (status != VF2_OK) {
        fprintf(stderr, "could not load maincpu from %s: %s\n",
                rom_dir, vf2_status_string(status));
        return EXIT_FAILURE;
    }
    status = vf2_parse_i960_boot_vectors(image, image_size, &vectors);
    if (status != VF2_OK) {
        fprintf(stderr, "boot vectors: %s\n", vf2_status_string(status));
        free(image);
        return EXIT_FAILURE;
    }

    status = vf2_i960_analysis_init(&analysis, image, image_size);
    if (status != VF2_OK) {
        fprintf(stderr, "analysis init: %s\n", vf2_status_string(status));
        free(image);
        return EXIT_FAILURE;
    }
    /* Boot + scheduler-task entries, so function boundaries are the real ones;
     * explicitly requested addresses and the backlog run starts are added too,
     * the latter because the executed code lives in functions reached only by
     * indirect dispatch, which the boot/task walk does not discover. Seeding
     * the observed addresses forces the analyser to bound a function there. */
    entry_count = discover_entries(image, image_size, &vectors,
                                   entries, VF2LIFT_MAX_ENTRIES);
    for (index = 0u; index < requested_count; ++index) {
        add_unique(entries, &entry_count, VF2LIFT_MAX_ENTRIES, requested[index]);
    }
    if (backlog_path != NULL) {
        backlog_start_count = read_backlog_starts(
            backlog_path, backlog_starts, VF2LIFT_MAX_ENTRIES);
        for (index = 0u; index < backlog_start_count; ++index) {
            add_unique(entries, &entry_count, VF2LIFT_MAX_ENTRIES,
                       backlog_starts[index]);
        }
    }
    status = vf2_i960_analyze(&analysis, entries, entry_count);
    if (status != VF2_OK) {
        fprintf(stderr, "analyze: %s\n", vf2_status_string(status));
        vf2_i960_analysis_destroy(&analysis);
        free(image);
        return EXIT_FAILURE;
    }
    if (backlog_path != NULL) {
        resolve_backlog_functions(backlog_starts, backlog_start_count, &analysis,
                                  requested, &requested_count,
                                  VF2LIFT_MAX_ENTRIES);
    }

    out_c = fopen(out_c_path, "w");
    out_h = fopen(out_h_path, "w");
    if (out_c == NULL || out_h == NULL) {
        fprintf(stderr, "cannot open outputs\n");
        if (out_c != NULL) { fclose(out_c); }
        if (out_h != NULL) { fclose(out_h); }
        vf2_i960_analysis_destroy(&analysis);
        free(image);
        return EXIT_FAILURE;
    }

    emit_banner(out_h, rom_dir);
    fprintf(out_h,
        "#ifndef VF2_LIFTED_GENERATED_H\n"
        "#define VF2_LIFTED_GENERATED_H\n\n"
        "#include <stddef.h>\n"
        "#include <stdint.h>\n\n"
        "#include \"vf2/lift/runtime.h\"\n\n");

    emit_banner(out_c, rom_dir);
    fprintf(out_c, "#include \"%s\"\n\n", path_basename(out_h_path));

    /* Bodies. In requested mode only the exact requested entries are emitted;
     * otherwise every discovered function. A declaration is written only for a
     * function whose body is emitted, so the table never references an
     * undefined symbol. */
    for (index = 0u; index < analysis.function_count; ++index) {
        const vf2_function *function = &analysis.functions[index];
        int wanted = (requested_count == 0u);
        size_t r = 0u;
        for (r = 0u; r < requested_count; ++r) {
            if (requested[r] == function->address) { wanted = 1; break; }
        }
        if (!wanted) {
            continue;
        }
        if (!function_is_liftable(&analysis, function)) {
            (void)emit_function(out_c, &analysis, function); /* writes a comment */
            continue;
        }
        fprintf(out_h, "vf2_status vf2_lifted_%08x("
                "vf2_model2a *machine, vf2_i960_cpu *cpu);\n",
                (unsigned)function->address);
        if (emit_function(out_c, &analysis, function)) {
            ++emitted;
        }
    }

    /* Registration table, in emission order (liftable, requested functions). */
    fprintf(out_c, "const vf2_lift_entry vf2_lift_table[] = {\n");
    for (index = 0u; index < analysis.function_count; ++index) {
        const vf2_function *function = &analysis.functions[index];
        int wanted = (requested_count == 0u);
        size_t r = 0u;
        for (r = 0u; r < requested_count; ++r) {
            if (requested[r] == function->address) { wanted = 1; break; }
        }
        if (!wanted || !function_is_liftable(&analysis, function)) {
            continue;
        }
        fprintf(out_c,
            "    { UINT32_C(0x%08x), UINT32_C(0x%08x), vf2_lifted_%08x, \"",
            (unsigned)function->address, (unsigned)function->end,
            (unsigned)function->address);
        emit_escaped(out_c, function->name);
        fprintf(out_c, "\" },\n");
    }
    fprintf(out_c,
        "};\n"
        "const size_t vf2_lift_table_count =\n"
        "    sizeof(vf2_lift_table) / sizeof(vf2_lift_table[0]);\n");

    fprintf(out_h,
        "\nextern const vf2_lift_entry vf2_lift_table[];\n"
        "extern const size_t vf2_lift_table_count;\n\n"
        "#endif\n");

    fclose(out_c);
    fclose(out_h);
    vf2_i960_analysis_destroy(&analysis);
    free(image);

    fprintf(stderr, "vf2lift: emitted %zu function(s) to %s\n",
            emitted, out_c_path);
    return EXIT_SUCCESS;
}
