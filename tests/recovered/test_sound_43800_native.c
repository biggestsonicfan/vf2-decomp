/* ====================================================================
 * 0x43800 sound-queue UART drainer ctest entry (v0825 8f)
 *
 * The drainer forwards one queued sound-command byte per call to
 * the Model 2 serial UART (0x01c80000, modeled as flat IO-control
 * RAM: accepted, no device effect) and applies all queue-state
 * work first:
 *
 *   Empty (entries == 0 on the load path): clean park at 0x43884.
 *   Send-direct (byte-idx <= 2): emits staged[idx] with no load.
 *   Entry-load (byte-idx > 2, entries > 0): stages entry[rcur],
 *     advances (count, rcur), emits staged[2]. Branch rule measured:
 *     the printed `cmpible 0` does not reproduce silicon (idx 1
 *     sends, idx 5/0xff load) -- send iff idx <= 2.
 *
 * The C performs the same stores for real, parks ip at the ret
 * slot without stepping it (no frame on either side), and counts
 * retired instructions only. r6 is never read.
 *
 * ROM-independent unit legs prove the invalid-arg guards and the
 * empty path. ROM-backed differential legs (needs argv[1]) stage
 * identical work RAM on two machines, run the C on one and
 * hand-step the reference stepper on the other to the terminal
 * (or its failure), and compare status + ip + r3/r4/r5/r6 +
 * instruction delta + full work RAM. No snapshot, no frame on
 * either side.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/hybrid.h"
#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/rom.h"
#include "vf2/status.h"

#define ENTRY_IP UINT32_C(0x00043800)
#define RET_IP UINT32_C(0x00043884)
#define STORE_IP UINT32_C(0x00043868)

#define Q_COUNT UINT32_C(0x00504001)
#define Q_RCUR UINT32_C(0x00504002)
#define Q_WCUR UINT32_C(0x00504003)
#define Q_STAGE UINT32_C(0x00504010)
#define Q_BIDX UINT32_C(0x00504014)
#define Q_BASE UINT32_C(0x00504020)
#define UART_DATA UINT32_C(0x01c80000)

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(                                                \
                stderr,                                             \
                "FAILED %s:%d: %s\n",                              \
                __FILE__,                                           \
                __LINE__,                                           \
                #expression                                         \
            );                                                      \
            ++failures;                                             \
        }                                                           \
    } while (0)

static void setup_cpu(vf2_i960_cpu *cpu)
{
    /* No frame: the recovery never steps the ret, and the reference
     * harness parks on ret arrival for direct comparison. r6 is the
     * live caller contract: uart-init (0x437d0) and filer-common
     * (0x438ec) both establish r6 = 0xe80004 (IRQ poke register)
     * before this code is reachable. */
    vf2_i960_cpu_reset(cpu, 0u, 0u, ENTRY_IP);
    cpu->registers[3] = UINT32_C(0xdead0003);
    cpu->registers[4] = UINT32_C(0xdead0004);
    cpu->registers[5] = UINT32_C(0xdead0005);
    cpu->registers[6] = UINT32_C(0x00e80004);
}

static void write_u8(vf2_model2a *machine, uint32_t address, uint8_t value)
{
    CHECK(vf2_model2a_write(machine, address, &value, sizeof(value)) ==
          VF2_OK);
}

static void write_u32(
    vf2_model2a *machine,
    uint32_t address,
    uint32_t value
)
{
    uint8_t bytes[4];
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8u);
    bytes[2] = (uint8_t)(value >> 16u);
    bytes[3] = (uint8_t)(value >> 24u);
    CHECK(vf2_model2a_write(machine, address, bytes, sizeof(bytes)) ==
          VF2_OK);
}

/* ROM-independent: invalid arguments fail closed. */
static void run_invalid(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    CHECK(vf2_model2a_initialize(&machine) != 0);
    setup_cpu(&cpu);
    CHECK(vf2_hybrid_sound_43800_execute(NULL, &cpu) ==
          VF2_ERROR_INVALID_ARGUMENT);
    CHECK(vf2_hybrid_sound_43800_execute(&machine, NULL) ==
          VF2_ERROR_INVALID_ARGUMENT);
    cpu.ip = UINT32_C(0x00043804);
    CHECK(vf2_hybrid_sound_43800_execute(&machine, &cpu) ==
          VF2_ERROR_INVALID_ARGUMENT);
    vf2_model2a_shutdown(&machine);
}

/* ROM-independent: empty path returns cleanly at the ret slot. */
static void run_empty_unit(void)
{
    vf2_model2a machine;
    vf2_i960_cpu cpu;
    uint64_t before = 0u;
    vf2_status status = VF2_OK;

    memset(&machine, 0, sizeof(machine));
    memset(&cpu, 0, sizeof(cpu));
    CHECK(vf2_model2a_initialize(&machine) != 0);
    setup_cpu(&cpu);
    write_u8(&machine, Q_BIDX, 0xffu);
    write_u8(&machine, Q_COUNT, 0u);
    before = cpu.executed_instructions;
    status = vf2_hybrid_sound_43800_execute(&machine, &cpu);
    CHECK(status == VF2_OK);
    CHECK(cpu.ip == RET_IP);
    /* ldib + cmpible + ldob + cmpobe = 4 (ret never stepped). */
    CHECK(cpu.executed_instructions - before == UINT64_C(4));
    CHECK(cpu.registers[6] == UINT32_C(0x00e80004));
    CHECK(cpu.registers[3] == 0u);
    CHECK(cpu.registers[5] == UINT32_C(0xffffffff));
    CHECK(cpu.registers[6] == UINT32_C(0x00e80004));
    vf2_model2a_shutdown(&machine);
}

typedef struct differential_expectation {
    const char *name;
    uint8_t byte_index;
    uint8_t entries;
    uint8_t read_cursor;
    uint32_t queue_entry;
    uint32_t staged;
} differential_expectation;

/* Hand-step the reference through the terminal ret (the fake
 * frame lands back on it). Every leg below ends VF2_OK: the UART
 * store and ack pokes land in modeled IO-control RAM. */
static vf2_status run_reference(
    vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    uint64_t *delta_out
)
{
    uint64_t before = cpu->executed_instructions;
    uint32_t steps = 0u;

    for (steps = 0u; steps < 200u; ++steps) {
        vf2_status status = vf2_i960_step(cpu, machine, NULL);
        if (status != VF2_OK) {
            *delta_out = cpu->executed_instructions - before;
            return status;
        }
        if (cpu->ip == RET_IP) {
            break;
        }
    }
    *delta_out = cpu->executed_instructions - before;
    if (cpu->ip != RET_IP) {
        return VF2_ERROR_UNSUPPORTED;
    }
    return VF2_OK;
}

static void stage_window(
    vf2_model2a *machine,
    const differential_expectation *leg
)
{
    write_u8(machine, Q_BIDX, leg->byte_index);
    write_u8(machine, Q_COUNT, leg->entries);
    write_u8(machine, Q_RCUR, leg->read_cursor);
    write_u32(machine, Q_BASE, leg->queue_entry);
    write_u32(machine, Q_STAGE, leg->staged);
}

static void run_differential_leg(
    const uint8_t *rom,
    size_t rom_size,
    const differential_expectation *leg
)
{
    vf2_model2a ref_machine;
    vf2_model2a nat_machine;
    vf2_i960_cpu ref_cpu;
    vf2_i960_cpu nat_cpu;
    vf2_status ref_status = VF2_OK;
    vf2_status nat_status = VF2_OK;
    uint64_t ref_delta = 0u;
    uint64_t nat_before = 0u;
    uint64_t nat_delta = 0u;

    memset(&ref_machine, 0, sizeof(ref_machine));
    memset(&nat_machine, 0, sizeof(nat_machine));
    memset(&ref_cpu, 0, sizeof(ref_cpu));
    memset(&nat_cpu, 0, sizeof(nat_cpu));
    CHECK(vf2_model2a_initialize(&ref_machine) != 0);
    CHECK(vf2_model2a_initialize(&nat_machine) != 0);
    if (ref_machine.work_ram == NULL || nat_machine.work_ram == NULL) {
        vf2_model2a_shutdown(&ref_machine);
        vf2_model2a_shutdown(&nat_machine);
        return;
    }
    CHECK(vf2_model2a_attach_main_rom(&ref_machine, rom, rom_size) ==
          VF2_OK);
    CHECK(vf2_model2a_attach_main_rom(&nat_machine, rom, rom_size) ==
          VF2_OK);
    setup_cpu(&ref_cpu);
    setup_cpu(&nat_cpu);
    stage_window(&ref_machine, leg);
    stage_window(&nat_machine, leg);
    ref_status = run_reference(&ref_machine, &ref_cpu, &ref_delta);
    nat_before = nat_cpu.executed_instructions;
    nat_status = vf2_hybrid_sound_43800_execute(&nat_machine, &nat_cpu);
    nat_delta = nat_cpu.executed_instructions - nat_before;
    if (nat_status != ref_status) {
        fprintf(stderr, "leg %s: status C=%d S=%d\n", leg->name,
                (int)nat_status, (int)ref_status);
        CHECK(0);
    }
    if (nat_cpu.ip != ref_cpu.ip) {
        fprintf(stderr, "leg %s: ip C=0x%05x S=0x%05x\n", leg->name,
                (unsigned)nat_cpu.ip, (unsigned)ref_cpu.ip);
        CHECK(0);
    }
    CHECK(nat_cpu.registers[3] == ref_cpu.registers[3]);
    CHECK(nat_cpu.registers[4] == ref_cpu.registers[4]);
    CHECK(nat_cpu.registers[5] == ref_cpu.registers[5]);
    /* r6 is the caller's poke register on both sides (never read by
     * the C, never written by either side). */
    CHECK(nat_cpu.registers[6] == UINT32_C(0x00e80004));
    CHECK(ref_cpu.registers[6] == UINT32_C(0x00e80004));
    if (nat_delta != ref_delta) {
        fprintf(stderr, "leg %s: ins C=%llu S=%llu\n", leg->name,
                (unsigned long long)nat_delta,
                (unsigned long long)ref_delta);
        CHECK(0);
    }
    CHECK(memcmp(ref_machine.work_ram, nat_machine.work_ram,
                 ref_machine.work_ram_size) == 0);
    fprintf(stderr, "drainer %s: status=%d ip=0x%05x r4=0x%02x ins=%llu\n",
            leg->name, (int)nat_status, (unsigned)nat_cpu.ip,
            (unsigned)(nat_cpu.registers[4] & 0xffu),
            (unsigned long long)nat_delta);
    fprintf(stderr, "  C: st=%d ip=0x%05x r3=%08x r4=%08x r5=%08x r6=%08x depth=%u\n",
            (int)nat_status, (unsigned)nat_cpu.ip,
            (unsigned)nat_cpu.registers[3], (unsigned)nat_cpu.registers[4],
            (unsigned)nat_cpu.registers[5], (unsigned)nat_cpu.registers[6],
            (unsigned)nat_cpu.local_frame_depth);
    fprintf(stderr, "  S: st=%d ip=0x%05x r3=%08x r4=%08x r5=%08x r6=%08x depth=%u\n",
            (int)ref_status, (unsigned)ref_cpu.ip,
            (unsigned)ref_cpu.registers[3], (unsigned)ref_cpu.registers[4],
            (unsigned)ref_cpu.registers[5], (unsigned)ref_cpu.registers[6],
            (unsigned)ref_cpu.local_frame_depth);
    vf2_model2a_shutdown(&ref_machine);
    vf2_model2a_shutdown(&nat_machine);
}

static void run_differential(const char *dir)
{
    /* idx == 0 sends staged[0] with no load. */
    static const differential_expectation direct = {
        "direct", 0u, 0u, 0u, UINT32_C(0x00ad1001), UINT32_C(0x00ad1001)
    };
    /* idx == 0xff loads entry[rcur], stages it, sends staged[2]. */
    static const differential_expectation load = {
        "load", 0xffu, 1u, 0u, UINT32_C(0x00ad1001), UINT32_C(0)
    };
    /* idx == 1 with entries live: load-vs-send discriminator. */
    static const differential_expectation mid = {
        "mid", 1u, 2u, 3u, UINT32_C(0x008e2950), UINT32_C(0x00ad1001)
    };
    /* idx == 2 with entries live: boundary of the send rule. */
    static const differential_expectation fresh = {
        "fresh", 2u, 1u, 7u, UINT32_C(0x00891e32), UINT32_C(0)
    };
    /* idx == 5, queue empty: sends staged[5] under ==0xff. */
    static const differential_expectation idle5 = {
        "idle5", 5u, 0u, 0u, UINT32_C(0), UINT32_C(0x00ad1001)
    };
    /* idx == 0x80: sign-bit discriminator (sends under ==0xff). */
    static const differential_expectation high = {
        "high", 0x80u, 0u, 0u, UINT32_C(0), UINT32_C(0x00ad1001)
    };
    /* idx == 3: range shape between the pinned points. */
    static const differential_expectation three = {
        "three", 3u, 0u, 0u, UINT32_C(0), UINT32_C(0x00ad1001)
    };
    /* Empty with idx == 0 takes the direct-send door (staged[0]). */
    static const differential_expectation empty0 = {
        "empty0", 0u, 0u, 0u, UINT32_C(0), UINT32_C(0x00ad1001)
    };
    uint8_t *rom = NULL;
    size_t rom_size = 0u;

    if (vf2_romset_build_region(dir, VF2_REGION_MAINCPU, &rom,
                                &rom_size) != VF2_OK ||
        rom == NULL) {
        fprintf(stderr, "FAILED: cannot build maincpu region from %s\n",
                dir);
        CHECK(0);
        free(rom);
        return;
    }
    run_differential_leg(rom, rom_size, &direct);
    run_differential_leg(rom, rom_size, &load);
    run_differential_leg(rom, rom_size, &mid);
    run_differential_leg(rom, rom_size, &fresh);
    run_differential_leg(rom, rom_size, &idle5);
    run_differential_leg(rom, rom_size, &high);
    run_differential_leg(rom, rom_size, &three);
    run_differential_leg(rom, rom_size, &empty0);
    free(rom);
}

int main(int argc, char **argv)
{
    run_invalid();
    run_empty_unit();
    if (argc == 2) {
        run_differential(argv[1]);
    } else {
        puts("sound-43800 ROM-independent passed");
        return failures == 0 ? 0 : 1;
    }
    if (failures != 0) {
        fprintf(stderr, "%d sound-43800 failed\n", failures);
    } else {
        puts("sound-43800 differential tests passed");
    }
    return failures == 0 ? 0 : 1;
}
