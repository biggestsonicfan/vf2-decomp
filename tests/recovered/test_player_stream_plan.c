#include "player_selector_scratch_test_support.inc"
#include "player_selector_scratch_test_machine.inc"
#include "player_selector_scratch_test_special.inc"
#include "player_selector_scratch_test_code2.inc"
#include "player_selector_scratch_test_counts.inc"

int main(void)
{
    scratch_test_code0_triple_copy_plan();
    scratch_test_code1_dynamic_cvtri_plan();
    scratch_test_code2_skips_second_float();
    scratch_test_code5_code6_counts_and_fixed_apply();
    scratch_test_nonfinite_preflight_is_transactional();

    if (failures != 0) {
        fprintf(stderr, "%d recovered player stream test(s) failed\n", failures);
        return 1;
    }
    printf("recovered player stream tests passed\n");
    return 0;
}
