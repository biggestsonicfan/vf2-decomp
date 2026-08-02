#ifndef VF2_ANALYSIS_ORCHESTRATOR_SCAN_H
#define VF2_ANALYSIS_ORCHESTRATOR_SCAN_H

#include <stddef.h>
#include <stdint.h>

#include "vf2/i960/executor.h"
#include "vf2/model2a.h"
#include "vf2/status.h"

enum {
    VF2_ORCHESTRATOR_RECORD_SCAN_ENTRY = 0x0004bd24u,
    VF2_ORCHESTRATOR_RECORD_SCAN_EXIT = 0x0004bf90u,
    VF2_ORCHESTRATOR_RECORD_START = 0x00550168u,
    VF2_ORCHESTRATOR_RECORD_END = 0x005502a8u,
    VF2_ORCHESTRATOR_RECORD_STRIDE = 0x20u,
    VF2_ORCHESTRATOR_RECORD_ACTIVE_OFFSET = 0x02u,
    VF2_ORCHESTRATOR_RECORD_COUNT = 10u
};

typedef struct vf2_orchestrator_scan_report {
    uint32_t entry_address;
    uint32_t exit_address;
    uint32_t first_record_address;
    uint32_t end_record_address;
    size_t records_scanned;
    uint64_t recovered_instruction_count;
    int cpu_poststate_applied;
} vf2_orchestrator_scan_report;

/*
 * Recover the observed final texture-record pass at 0x0004bd24. The accepted
 * path scans ten inactive 0x20-byte records and branches to 0x0004bf90. An
 * active record is deliberately rejected until its processing branch has its
 * own complete CPU and memory proof.
 */
vf2_status vf2_orchestrator_scan_inactive_records(
    const vf2_model2a *machine,
    vf2_i960_cpu *cpu,
    vf2_orchestrator_scan_report *report
);

#endif
