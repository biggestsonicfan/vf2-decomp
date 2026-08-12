/* Observed sixth-dispatch player pose corridor: 0x16504 -> 0x14418. */
#define vf2_hybrid_i960_run_tail vf2_hybrid_i960_run_tail_previous
#include "player_i960_bridge_tail_previous.inc"
#undef vf2_hybrid_i960_run_tail

#include "player_i960_bridge_pose_data.inc"
#define vf2_hybrid_i960_run_tail vf2_hybrid_i960_run_tail_pose
#include "player_i960_bridge_pose_logic.inc"
#undef vf2_hybrid_i960_run_tail

/* Complete player state update immediately following the pose corridor. */
#include "player_i960_bridge_status_logic.inc"
