#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "vf2/status.h"

#include "../../src/recovered/player_27130_types.inc"
#include "../../src/recovered/player_27130_classify.inc"

static int failures = 0;

#define CHECK(expression)                                           \
    do {                                                            \
        if (!(expression)) {                                        \
            fprintf(stderr, "FAILED %s:%d: %s\n",                 \
                    __FILE__, __LINE__, #expression);              \
            ++failures;                                             \
        }                                                           \
    } while (0)

static vf2_player_27130_facts baseline_facts(void)
{
    vf2_player_27130_facts facts;
    memset(&facts, 0, sizeof(facts));
    facts.player_flags = UINT32_C(1) << 26u;
    facts.selector_value = UINT16_C(5);
    return facts;
}

static void test_immediate_early_paths(void)
{
    vf2_player_27130_facts facts = baseline_facts();
    vf2_player_27130_plan plan;

    facts.global_gate = UINT8_C(1);
    CHECK(player_27130_classify(&facts, &plan) == VF2_OK);
    CHECK(plan.outcome == VF2_PLAYER_27130_EARLY_RETURN);
    CHECK(plan.mode == 0u);

    facts = baseline_facts();
    facts.player_flags = 0u;
    CHECK(player_27130_classify(&facts, &plan) == VF2_OK);
    CHECK(plan.outcome == VF2_PLAYER_27130_EARLY_RETURN);
    CHECK(plan.mode == 0u);

    facts = baseline_facts();
    facts.previous_be4 = UINT8_C(1);
    CHECK(player_27130_classify(&facts, &plan) == VF2_OK);
    CHECK(plan.outcome == VF2_PLAYER_27130_EARLY_RETURN);
    CHECK(plan.mode == 0u);
}

static void test_mode2_return(void)
{
    vf2_player_27130_facts facts = baseline_facts();
    vf2_player_27130_plan plan;

    facts.state = UINT32_C(1) << 17u;
    CHECK(player_27130_classify(&facts, &plan) == VF2_OK);
    CHECK(plan.outcome == VF2_PLAYER_27130_MODE2_RETURN);
    CHECK(plan.mode == UINT8_C(2));
    CHECK(plan.bucket == UINT16_C(4));
    CHECK(plan.remainder == UINT32_C(1));
}

static void test_literal_bucket_fallthrough(void)
{
    vf2_player_27130_facts facts = baseline_facts();
    vf2_player_27130_plan plan;

    facts.selector_value = UINT16_C(3);
    CHECK(player_27130_classify(&facts, &plan) == VF2_OK);
    CHECK(plan.outcome == VF2_PLAYER_27130_HEAVY_CONTINUATION);
    CHECK(plan.mode == UINT8_C(3));
    CHECK(plan.bucket == UINT16_C(4));
    CHECK(plan.remainder == UINT32_MAX);

    facts.selector_value = UINT16_C(16);
    CHECK(player_27130_classify(&facts, &plan) == VF2_OK);
    CHECK(plan.bucket == UINT16_C(4));
    CHECK(plan.remainder == UINT32_C(12));

    facts.selector_value = UINT16_C(17);
    CHECK(player_27130_classify(&facts, &plan) == VF2_OK);
    CHECK(plan.bucket == UINT16_C(8));
    CHECK(plan.remainder == UINT32_C(9));
}

static void test_mode1_sources_and_early_filters(void)
{
    vf2_player_27130_facts facts = baseline_facts();
    vf2_player_27130_plan plan;

    facts.selector_state = UINT32_C(1) << 18u;
    CHECK(player_27130_classify(&facts, &plan) == VF2_OK);
    CHECK(plan.mode == UINT8_C(1));
    CHECK(plan.outcome == VF2_PLAYER_27130_HEAVY_CONTINUATION);

    facts = baseline_facts();
    facts.selector_byte = UINT8_C(0x7f);
    CHECK(player_27130_classify(&facts, &plan) == VF2_OK);
    CHECK(plan.mode == UINT8_C(1));

    facts = baseline_facts();
    facts.state = UINT32_C(0x00006600);
    CHECK(player_27130_classify(&facts, &plan) == VF2_OK);
    CHECK(plan.mode == UINT8_C(1));

    facts = baseline_facts();
    facts.old_selector_state = UINT32_C(1) << 23u;
    CHECK(player_27130_classify(&facts, &plan) == VF2_OK);
    CHECK(plan.outcome == VF2_PLAYER_27130_EARLY_RETURN);

    facts = baseline_facts();
    facts.old_selector_state = UINT32_C(0x00008040);
    CHECK(player_27130_classify(&facts, &plan) == VF2_OK);
    CHECK(plan.outcome == VF2_PLAYER_27130_EARLY_RETURN);

    facts = baseline_facts();
    facts.selector_byte = UINT8_C(4);
    CHECK(player_27130_classify(&facts, &plan) == VF2_OK);
    CHECK(plan.outcome == VF2_PLAYER_27130_EARLY_RETURN);
}

static void test_selector_small_early_preserves_mode(void)
{
    vf2_player_27130_facts facts = baseline_facts();
    vf2_player_27130_plan plan;

    facts.state = UINT32_C(1) << 17u;
    facts.selector_value = UINT16_C(2);
    CHECK(player_27130_classify(&facts, &plan) == VF2_OK);
    CHECK(plan.outcome == VF2_PLAYER_27130_EARLY_RETURN);
    CHECK(plan.mode == UINT8_C(2));
    CHECK(plan.bucket == 0u);
    CHECK(plan.remainder == 0u);
}

static void test_mode_bit_clears(void)
{
    vf2_player_27130_facts facts = baseline_facts();
    vf2_player_27130_plan plan;

    facts.threshold_enabled = UINT8_C(1);
    facts.threshold = UINT16_C(2);
    CHECK(player_27130_classify(&facts, &plan) == VF2_OK);
    CHECK(plan.mode == UINT8_C(1));
    CHECK(plan.outcome == VF2_PLAYER_27130_HEAVY_CONTINUATION);

    facts = baseline_facts();
    facts.previous_be5 = UINT8_C(2);
    facts.previous_bdd = UINT8_C(2);
    CHECK(player_27130_classify(&facts, &plan) == VF2_OK);
    CHECK(plan.mode == UINT8_C(2));
    CHECK(plan.outcome == VF2_PLAYER_27130_MODE2_RETURN);

    facts = baseline_facts();
    facts.threshold_enabled = UINT8_C(1);
    facts.threshold = UINT16_C(2);
    facts.previous_be5 = UINT8_C(2);
    facts.previous_bdd = UINT8_C(2);
    CHECK(player_27130_classify(&facts, &plan) == VF2_OK);
    CHECK(plan.mode == 0u);
    CHECK(plan.outcome == VF2_PLAYER_27130_HEAVY_CONTINUATION);
}

int main(void)
{
    test_immediate_early_paths();
    test_mode2_return();
    test_literal_bucket_fallthrough();
    test_mode1_sources_and_early_filters();
    test_selector_small_early_preserves_mode();
    test_mode_bit_clears();

    if (failures != 0) {
        fprintf(stderr, "%d player 27130 classifier test(s) failed\n", failures);
        return 1;
    }
    printf("player 27130 classifier tests passed\n");
    return 0;
}
