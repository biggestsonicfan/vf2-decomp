/* ====================================================================
 * 0x53750 display_profile_load ctest entry (v0780)
 *
 * Exercises the recovered display-profile load
 * vf2_hybrid_display_profile_load_53750_execute which:
 *   1. Reads src_base from main_data 0x0201fe68 into g3.
 *   2. Sets g4 = 0x00560000 (work_ram).
 *   3. Copies 0xb01 16-byte quads (0xb010 = 45072 bytes) from
 *      src[r3*16] to dst[r3*16] for r3 = 0..0xb00 inclusive
 *      (cmpinco/bg loop with r4=0xb00).
 *   4. Post-init work_ram 0x5001a0..0x5001bf:
 *        0x5001a0 = 0 (u32)
 *        0x5001a4 = 0x7f7f7f7f (u32)
 *        0x5001a8..0x5001af = 0 (u64, 8 bytes)
 *        0x5001b0 = 0 (u32)
 *        0x5001b4 = 0x7f7f7f7f (u32)
 *        0x5001b8..0x5001bf = 0 (u64, 8 bytes)
 *
 * Per vf2i960 disasm (108 B, 3 blocks, 22 instr):
 *   block_00053750 (init + src_base load)
 *   block_00053768 (16-byte quad-copy loop, cmpinco + bg)
 *   block_00053778 (post-init)
 *
 * Note: the loop is r3=0..0xb00 (0xb01 iters), so the dst region
 * is 0x560000..0x56b00f (45072 bytes). For testing, we use
 * src_base = 0x02000000 (main_data base) so the source reads
 * stay within the 0x20000-byte attached buffer.
 *
 * Legs:
 *   A: src_base = 0x02000000, fill_byte = 0xaa -> verify
 *      dst[0x560000..0x56b00f] is 0xaa.
 *   B: src_base = 0x02000000, fill_byte = 0xfe -> verify
 *      dst bytes = 0xfe.
 *   C: src_base = 0x02000000, fill_byte = 0x5a, post-init
 *      sentinel = 0xff -> verify dst and post-init regions.
 *   D: src_base = 0x02000000, fill_byte = 0x77, post-init
 *      sentinel = 0x33 -> verify the post-init overwrites them.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/hybrid/player.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

#define ENTRY_IP    UINT32_C(0x00053750)
#define POST_RET_IP UINT32_C(0x000537bc)
#define SRC_PTR_ADDR UINT32_C(0x0201fe68)
#define SRC_OFFSET_IN_MAIN_DATA (SRC_PTR_ADDR - UINT32_C(0x02000000))
#define DST_BASE    UINT32_C(0x00560000)
#define POST_INIT_BASE UINT32_C(0x005001a0)
#define MAIN_DATA_SIZE 0x20000u

static uint8_t main_data[MAIN_DATA_SIZE];

static int failures = 0;
#define CHECK(expr)                                                   \
    do {                                                              \
        if (!(expr)) {                                                \
            fprintf(stderr, "FAILED %s:%d: %s\n",                     \
                __FILE__, __LINE__, #expr);                           \
            ++failures;                                               \
        }                                                             \
    } while (0)

static void put_u32(vf2_model2a *m, uint32_t a, uint32_t v) {
    CHECK(vf2_model2a_write(m, a, &v, sizeof(v)) == VF2_OK);
}
static void put_u8(vf2_model2a *m, uint32_t a, uint8_t v) {
    CHECK(vf2_model2a_write(m, a, &v, sizeof(v)) == VF2_OK);
}
static void put_u8_md(uint8_t *md, uint32_t off, uint8_t v) {
    md[off] = v;
}
static void put_u32_md(uint8_t *md, uint32_t off, uint32_t v) {
    md[off + 0u] = (uint8_t)(v & 0xffu);
    md[off + 1u] = (uint8_t)((v >> 8) & 0xffu);
    md[off + 2u] = (uint8_t)((v >> 16) & 0xffu);
    md[off + 3u] = (uint8_t)((v >> 24) & 0xffu);
}
static uint32_t get_u32(vf2_model2a *m, uint32_t a) {
    uint32_t v = 0u;
    CHECK(vf2_model2a_read(m, a, &v, sizeof(v)) == VF2_OK);
    return v;
}
static uint8_t get_u8(vf2_model2a *m, uint32_t a) {
    uint8_t v = 0u;
    CHECK(vf2_model2a_read(m, a, &v, sizeof(v)) == VF2_OK);
    return v;
}

static void run_leg(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint32_t src_base,
    uint8_t fill_byte,
    uint8_t post_init_sentinel)
{
    vf2_status status;
    uint32_t i;

    /* Set src_base in main_data directly (main_data is
       read-only at the model2a boundary, so we write to
       the attached buffer directly). */
    put_u32_md(main_data, SRC_OFFSET_IN_MAIN_DATA, src_base);

    /* Fill 0xb010 bytes of source data with fill_byte (the
       loop is r3=0..0xb00 inclusive, so 0xb01 quads = 0xb010
       bytes). */
    for (i = 0u; i < UINT32_C(0xb010); ++i) {
        put_u8_md(main_data, (src_base - UINT32_C(0x02000000)) + i,
                  fill_byte);
    }

    /* Pre-fill post-init regions with sentinel. */
    for (i = 0u; i < 32u; ++i) {
        put_u8(machine, POST_INIT_BASE + i, post_init_sentinel);
    }

    /* Run the recovery. */
    cpu->ip = ENTRY_IP;
    CHECK(vf2_i960_cpu_enter_procedure(
        cpu, ENTRY_IP, POST_RET_IP) == VF2_OK);
    status = vf2_hybrid_display_profile_load_53750_execute(
        machine, cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu->ip == POST_RET_IP);

    /* Verify the 0xb010 bytes were copied to dst. Sample
       the first quad, the last quad, and a few mid-points. */
    CHECK(get_u8(machine, DST_BASE + 0u) == fill_byte);
    CHECK(get_u8(machine, DST_BASE + 15u) == fill_byte);
    CHECK(get_u8(machine, DST_BASE + 0x5000u) == fill_byte);
    CHECK(get_u8(machine, DST_BASE + 0xa000u) == fill_byte);
    CHECK(get_u8(machine, DST_BASE + 0xb000u) == fill_byte);
    CHECK(get_u8(machine, DST_BASE + 0xb00fu) == fill_byte);

    /* Verify post-init regions. */
    CHECK(get_u32(machine, POST_INIT_BASE + 0u) == 0u);
    CHECK(get_u32(machine, POST_INIT_BASE + 4u) == 0x7f7f7f7fu);
    CHECK(get_u32(machine, POST_INIT_BASE + 8u) == 0u);
    CHECK(get_u32(machine, POST_INIT_BASE + 12u) == 0u);
    CHECK(get_u32(machine, POST_INIT_BASE + 16u) == 0u);
    CHECK(get_u32(machine, POST_INIT_BASE + 20u) == 0x7f7f7f7fu);
    CHECK(get_u32(machine, POST_INIT_BASE + 24u) == 0u);
    CHECK(get_u32(machine, POST_INIT_BASE + 28u) == 0u);
}

int main(void) {
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    if (vf2_model2a_initialize(&machine) == 0) return 1;
    /* Attach main_data so src_base and src reads work. */
    CHECK(vf2_model2a_attach_main_data(
        &machine, main_data, MAIN_DATA_SIZE) == VF2_OK);
    vf2_i960_cpu_reset(&cpu, 0u, 0u, ENTRY_IP);

    /* ====================================================================
     * Leg A: src_base=0x02000000 (main_data base), fill_byte=0xaa.
     */
    run_leg(&machine, &cpu,
            /*src_base*/UINT32_C(0x02000000),
            /*fill_byte*/0xaau,
            /*post_init_sentinel*/0xffu);
    printf("  Leg A: src=0x02000000 0xaa fill -> dst 0x560000 0xaa\n");

    /* ====================================================================
     * Leg B: src_base=0x02000000, fill_byte=0xfe.
     */
    run_leg(&machine, &cpu,
            /*src_base*/UINT32_C(0x02000000),
            /*fill_byte*/0xfeu,
            /*post_init_sentinel*/0u);
    printf("  Leg B: src=0x02000000 0xfe fill -> dst 0x560000 0xfe\n");

    /* ====================================================================
     * Leg C: src_base=0x02000000 with 0x5a fill, sentinel 0xff.
     */
    run_leg(&machine, &cpu,
            /*src_base*/UINT32_C(0x02000000),
            /*fill_byte*/0x5au,
            /*post_init_sentinel*/0xffu);
    printf("  Leg C: src=0x02000000 0x5a fill -> dst 0x560000 0x5a\n");

    /* ====================================================================
     * Leg D: src_base=0x02000000, fill_byte=0x77, post-init
     * sentinel 0x33.
     */
    run_leg(&machine, &cpu,
            /*src_base*/UINT32_C(0x02000000),
            /*fill_byte*/0x77u,
            /*post_init_sentinel*/0x33u);
    printf("  Leg D: src=0x02000000 0x77 fill -> post-init "
           "regions overwritten\n");

    vf2_model2a_shutdown(&machine);

    if (failures != 0) {
        fprintf(stderr, "FAILED: 0x53750 (%d failures)\n", failures);
        return 1;
    }
    printf("ok: 0x53750 display_profile_load (4 legs)\n");
    return 0;
}