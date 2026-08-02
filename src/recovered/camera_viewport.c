#include "vf2/recovered.h"

#include <limits.h>
#include <string.h>

#define CAMERA_RANGE_SCALE UINT32_C(0x0050a00c)
#define CAMERA_RANGE_CENTER UINT32_C(0x0050a014)
#define CAMERA_RANGE_DIVISOR UINT32_C(0x0050a024)
#define CAMERA_RANGE_LOW UINT32_C(0x0050a0d0)
#define CAMERA_RANGE_HIGH UINT32_C(0x0050a0d4)
#define CAMERA_FIRST_NORMALIZED_LOW UINT32_C(0x0050a0d8)
#define CAMERA_FIRST_NORMALIZED_HIGH UINT32_C(0x0050a0dc)
#define CAMERA_FIGHTER0 UINT32_C(0x00500804)
#define CAMERA_FIGHTER1 UINT32_C(0x00500808)
#define CAMERA_FIGHTER0_PROFILE UINT32_C(0x0050109c)
#define CAMERA_FIGHTER1_PROFILE UINT32_C(0x005010a0)
#define CAMERA_PROJECTION_CENTER UINT32_C(0x005010a4)
#define CAMERA_PROJECTION_START UINT32_C(0x005010a8)
#define CAMERA_PROJECTION_END UINT32_C(0x005010e4)
#define CAMERA_FIGHTER0_WEIGHT UINT32_C(0x005010e8)
#define CAMERA_FIGHTER1_WEIGHT UINT32_C(0x005010ea)
#define CAMERA_PROJECTION_WEIGHT UINT32_C(0x005010ec)
#define CAMERA_RUNTIME_FLAGS UINT32_C(0x00500068)
#define CAMERA_INPUT_INDEX UINT32_C(0x00500064)
#define CAMERA_INPUT_TABLE UINT32_C(0x0006eea0)
#define CAMERA_TABLE_POINTERS_8 UINT32_C(0x0006e648)
#define CAMERA_TABLE_POINTERS_10_A UINT32_C(0x0006e998)
#define CAMERA_TABLE_POINTERS_10_B UINT32_C(0x0006e9c0)
#define CAMERA_SCRATCH_ADDRESS (VF2_COPRO_PORT_BASE + UINT32_C(0x4000))

static int viewport_registry_range_valid(uint32_t registry_address, uint32_t required_size)
{
    const uint64_t start = registry_address;
    const uint64_t end = start + required_size;
    const uint64_t work_start = VF2_WORK_RAM_BASE;
    const uint64_t work_end = work_start + VF2_WORK_RAM_SIZE;
    return start >= work_start && end <= work_end;
}

static float viewport_bits_to_float(uint32_t bits)
{
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t viewport_float_to_bits(float value)
{
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t viewport_add(uint32_t first, uint32_t second)
{
    const float result = viewport_bits_to_float(first) + viewport_bits_to_float(second);
    return viewport_float_to_bits(result);
}

static uint32_t viewport_sub(uint32_t first, uint32_t second)
{
    const float result = viewport_bits_to_float(first) - viewport_bits_to_float(second);
    return viewport_float_to_bits(result);
}

static uint32_t viewport_mul(uint32_t first, uint32_t second)
{
    const float result = viewport_bits_to_float(first) * viewport_bits_to_float(second);
    return viewport_float_to_bits(result);
}

static uint32_t viewport_div(uint32_t first, uint32_t second)
{
    const float result = viewport_bits_to_float(first) / viewport_bits_to_float(second);
    return viewport_float_to_bits(result);
}

static int viewport_is_negative(uint32_t bits)
{
    return (bits & UINT32_C(0x80000000)) != 0u;
}

static vf2_status viewport_read_u16(
    const vf2_model2a *machine,
    uint32_t address,
    uint16_t *value
)
{
    uint8_t bytes[2];
    vf2_status status = VF2_OK;
    if (machine == NULL || value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(machine, address, bytes, sizeof(bytes));
    if (status == VF2_OK) {
        *value = (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8u));
    }
    return status;
}

static vf2_status viewport_write_u16(
    vf2_model2a *machine,
    uint32_t address,
    uint16_t value
)
{
    uint8_t bytes[2];
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8u);
    return vf2_model2a_write(machine, address, bytes, sizeof(bytes));
}

static vf2_status viewport_round_to_i32(uint32_t bits, int32_t *value)
{
    const float input = viewport_bits_to_float(bits);
    float adjusted = 0.0f;
    if (value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (!(input >= -2147483648.0f && input <= 2147483520.0f)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    adjusted = input < 0.0f ? input - 0.5f : input + 0.5f;
    *value = (int32_t)adjusted;
    return VF2_OK;
}

vf2_status vf2_recovered_camera_range_window(
    vf2_model2a *machine,
    uint32_t half_width,
    uint32_t *lower,
    uint32_t *upper,
    uint32_t *center
)
{
    uint32_t center_value = 0u;
    uint32_t lower_value = 0u;
    uint32_t upper_value = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || lower == NULL || upper == NULL || center == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_write_u32(machine, CAMERA_SCRATCH_ADDRESS, half_width);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, CAMERA_RANGE_CENTER, &center_value);
    }
    if (status == VF2_OK) {
        lower_value = viewport_sub(center_value, half_width);
        upper_value = viewport_add(center_value, half_width);
        status = vf2_model2a_write_u32(machine, CAMERA_RANGE_LOW, lower_value);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, CAMERA_RANGE_HIGH, upper_value);
    }
    if (status == VF2_OK) {
        *lower = lower_value;
        *upper = upper_value;
        *center = center_value;
    }
    return status;
}

static vf2_status camera_project_one_fighter(
    vf2_model2a *machine,
    uint32_t fighter,
    uint32_t low_offset,
    uint32_t high_offset,
    uint32_t input_profile,
    uint32_t *output_profile,
    int32_t *output_weight
)
{
    uint32_t projection_center = 0u;
    uint32_t lower_bound = 0u;
    uint32_t upper_bound = 0u;
    uint32_t fighter_position = 0u;
    uint32_t lower_distance = 0u;
    uint32_t upper_distance = 0u;
    uint32_t nearest_distance = 0u;
    uint32_t doubled_center = 0u;
    uint32_t numerator = 0u;
    uint32_t denominator = 0u;
    uint32_t interpolation_start = 0u;
    uint32_t interpolation_end = 0u;
    uint32_t low_weight = 0u;
    uint32_t high_weight = 0u;
    uint32_t ratio = 0u;
    uint32_t multiplier = 0u;
    uint32_t result_profile = input_profile;
    uint32_t result_weight_bits = 0u;
    int32_t result_weight = 0;
    uint16_t configured_weight = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || output_profile == NULL || output_weight == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(machine, CAMERA_PROJECTION_CENTER, &projection_center);
    if (status == VF2_OK) {
        lower_bound = viewport_sub(low_offset, projection_center);
        upper_bound = viewport_add(projection_center, high_offset);
        status = vf2_model2a_read_u32(machine, fighter + UINT32_C(0x1f4), &fighter_position);
    }
    if (status != VF2_OK) {
        return status;
    }

    lower_distance = viewport_sub(fighter_position, lower_bound);
    if (viewport_is_negative(lower_distance)) {
        *output_profile = input_profile;
        *output_weight = 0;
        return VF2_OK;
    }
    upper_distance = viewport_sub(upper_bound, fighter_position);
    if (viewport_is_negative(upper_distance)) {
        *output_profile = input_profile;
        *output_weight = 0;
        return VF2_OK;
    }

    doubled_center = viewport_mul(projection_center, UINT32_C(0x40000000));
    nearest_distance = viewport_bits_to_float(lower_distance) <=
                       viewport_bits_to_float(upper_distance)
        ? lower_distance : upper_distance;
    numerator = viewport_sub(doubled_center, nearest_distance);

    if (viewport_is_negative(numerator)) {
        const uint32_t window_width = viewport_sub(upper_bound, lower_bound);
        const uint32_t half_width = viewport_mul(window_width, UINT32_C(0x3f000000));
        numerator = viewport_sub(nearest_distance, half_width);
        denominator = viewport_sub(doubled_center, half_width);
        status = vf2_model2a_read_u32(
            machine, CAMERA_PROJECTION_START, &interpolation_start
        );
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, CAMERA_PROJECTION_END, &interpolation_end
            );
        }
        if (status == VF2_OK) {
            status = viewport_read_u16(
                machine, CAMERA_PROJECTION_WEIGHT, &configured_weight
            );
        }
        low_weight = viewport_float_to_bits((float)configured_weight);
        high_weight = viewport_float_to_bits(16384.0f);
    } else {
        denominator = doubled_center;
        interpolation_start = UINT32_C(0x3f800000);
        status = vf2_model2a_read_u32(
            machine, CAMERA_PROJECTION_START, &interpolation_end
        );
        low_weight = UINT32_C(0x00000000);
        if (status == VF2_OK) {
            status = viewport_read_u16(
                machine, CAMERA_PROJECTION_WEIGHT, &configured_weight
            );
        }
        high_weight = viewport_float_to_bits((float)configured_weight);
    }
    if (status != VF2_OK) {
        return status;
    }
    if (viewport_bits_to_float(denominator) == 0.0f) {
        return VF2_ERROR_UNSUPPORTED;
    }

    ratio = viewport_div(numerator, denominator);
    multiplier = viewport_add(
        viewport_mul(ratio, viewport_sub(interpolation_start, interpolation_end)),
        interpolation_end
    );
    result_profile = viewport_mul(input_profile, multiplier);
    result_weight_bits = viewport_add(
        viewport_mul(ratio, viewport_sub(high_weight, low_weight)),
        high_weight
    );
    status = viewport_round_to_i32(result_weight_bits, &result_weight);
    if (status != VF2_OK) {
        return status;
    }
    if (nearest_distance != lower_distance) {
        result_weight = -result_weight;
    }

    *output_profile = result_profile;
    *output_weight = result_weight;
    return VF2_OK;
}

vf2_status vf2_recovered_camera_project_fighter_ranges(
    vf2_model2a *machine,
    uint32_t low_offset,
    uint32_t high_offset,
    vf2_recovered_camera_projection_report *report
)
{
    vf2_recovered_camera_projection_report local_report;
    uint32_t fighter0 = 0u;
    uint32_t fighter1 = 0u;
    uint32_t profile0 = 0u;
    uint32_t profile1 = 0u;
    int32_t weight0 = 0;
    int32_t weight1 = 0;
    vf2_status status = VF2_OK;

    if (machine == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    status = vf2_model2a_read_u32(machine, CAMERA_FIGHTER0, &fighter0);
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, CAMERA_FIGHTER1, &fighter1);
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, CAMERA_FIGHTER0_PROFILE, &profile0);
    }
    if (status == VF2_OK) {
        status = camera_project_one_fighter(
            machine, fighter0, low_offset, high_offset,
            profile0, &profile0, &weight0
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, CAMERA_FIGHTER0_PROFILE, profile0);
    }
    if (status == VF2_OK) {
        status = viewport_write_u16(
            machine, CAMERA_FIGHTER0_WEIGHT, (uint16_t)weight0
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_read_u32(machine, CAMERA_FIGHTER1_PROFILE, &profile1);
    }
    if (status == VF2_OK) {
        status = camera_project_one_fighter(
            machine, fighter1, low_offset, high_offset,
            profile1, &profile1, &weight1
        );
    }
    if (status == VF2_OK) {
        status = vf2_model2a_write_u32(machine, CAMERA_FIGHTER1_PROFILE, profile1);
    }
    if (status == VF2_OK) {
        status = viewport_write_u16(
            machine, CAMERA_FIGHTER1_WEIGHT, (uint16_t)weight1
        );
    }
    if (status == VF2_OK) {
        local_report.helper_address = UINT32_C(0x0001eff0);
        local_report.low_offset = low_offset;
        local_report.high_offset = high_offset;
        local_report.fighter0_profile = profile0;
        local_report.fighter1_profile = profile1;
        local_report.fighter0_weight = weight0;
        local_report.fighter1_weight = weight1;
        if (report != NULL) {
            *report = local_report;
        }
    }
    return status;
}

vf2_status vf2_recovered_camera_fill_viewport_table(
    vf2_model2a *machine,
    uint32_t lower,
    uint32_t upper,
    uint32_t entry_count,
    uint32_t step,
    uint32_t destination,
    uint32_t clamp_low,
    uint32_t clamp_high,
    size_t *entries_written
)
{
    uint32_t remaining = entry_count;
    uint32_t current_lower = lower;
    uint32_t current_upper = upper;
    uint32_t runtime_flags = 0u;
    size_t written = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL || entry_count == 0u || entry_count > 64u) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (!viewport_registry_range_valid(destination, entry_count * 4u)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read_u32(machine, CAMERA_RUNTIME_FLAGS, &runtime_flags);

    while (status == VF2_OK && remaining != 0u) {
        const uint32_t output_index = entry_count - remaining;
        uint32_t pointer_address = 0u;
        uint32_t table = 0u;
        uint32_t table_index = 0u;
        uint32_t table_value = 0u;
        uint32_t interpolation_step = UINT32_C(0x3d4ccccd);
        uint32_t upper_table_index = 44u;
        int32_t rounded = 0;

        if (entry_count == 8u) {
            uint32_t scale = 0u;
            uint32_t ratio = 0u;
            pointer_address = CAMERA_TABLE_POINTERS_8 + output_index * 4u;
            status = vf2_model2a_read_u32(machine, CAMERA_RANGE_SCALE, &scale);
            if (status == VF2_OK && viewport_bits_to_float(scale) == 0.0f) {
                status = VF2_ERROR_UNSUPPORTED;
            }
            if (status == VF2_OK) {
                ratio = viewport_div(UINT32_C(0x40c00000), scale);
                interpolation_step = viewport_mul(UINT32_C(0x3d4ccccd), ratio);
                upper_table_index = 32u;
            }
        } else {
            pointer_address = ((runtime_flags & (UINT32_C(1) << 4u)) != 0u
                ? CAMERA_TABLE_POINTERS_10_B : CAMERA_TABLE_POINTERS_10_A) +
                output_index * 4u;
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, pointer_address, &table);
        }
        if (status != VF2_OK) {
            break;
        }

        if (viewport_bits_to_float(current_lower) >= viewport_bits_to_float(clamp_high) ||
            viewport_bits_to_float(current_upper) <= viewport_bits_to_float(clamp_low)) {
            table_index = 0u;
        } else if (viewport_bits_to_float(current_lower) < viewport_bits_to_float(clamp_low)) {
            uint32_t ratio = 0u;
            if (viewport_bits_to_float(interpolation_step) == 0.0f) {
                status = VF2_ERROR_UNSUPPORTED;
                break;
            }
            ratio = viewport_div(
                viewport_sub(clamp_low, current_lower), interpolation_step
            );
            status = viewport_round_to_i32(ratio, &rounded);
            if (status == VF2_OK && rounded < 0) {
                status = VF2_ERROR_UNSUPPORTED;
            }
            if (status == VF2_OK) {
                table_index = (uint32_t)rounded + upper_table_index;
            }
        } else if (viewport_bits_to_float(current_upper) <=
                   viewport_bits_to_float(clamp_high)) {
            table_index = upper_table_index;
        } else {
            uint32_t ratio = 0u;
            if (viewport_bits_to_float(interpolation_step) == 0.0f) {
                status = VF2_ERROR_UNSUPPORTED;
                break;
            }
            ratio = viewport_div(
                viewport_sub(clamp_high, current_lower), interpolation_step
            );
            status = viewport_round_to_i32(ratio, &rounded);
            if (status == VF2_OK && rounded < 0) {
                status = VF2_ERROR_UNSUPPORTED;
            }
            if (status == VF2_OK) {
                table_index = (uint32_t)rounded;
            }
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(
                machine, table + table_index * 4u, &table_value
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, destination + output_index * 4u, table_value
            );
        }
        if (status == VF2_OK) {
            ++written;
            current_lower = current_upper;
            current_upper = viewport_add(current_upper, step);
            --remaining;
        }
    }

    if (entries_written != NULL) {
        *entries_written = written;
    }
    return status;
}

static vf2_status camera_write_fixed_table(
    vf2_model2a *machine,
    uint32_t destination,
    const uint32_t *values,
    size_t count
)
{
    size_t index = 0u;
    vf2_status status = VF2_OK;
    for (index = 0u; status == VF2_OK && index < count; ++index) {
        status = vf2_model2a_write_u32(
            machine, destination + (uint32_t)index * 4u, values[index]
        );
    }
    return status;
}

static int camera_select_thresholds(
    uint32_t lower,
    uint32_t upper,
    uint32_t *low_threshold,
    uint32_t *high_threshold
)
{
    const float lower_value = viewport_bits_to_float(lower);
    const float upper_value = viewport_bits_to_float(upper);
    if (lower_value > -12.0f) {
        return 0;
    }
    if (upper_value > -22.0f) {
        *low_threshold = UINT32_C(0xc1b00000);
        *high_threshold = UINT32_C(0xc1400000);
        return 1;
    }
    if (lower_value > -60.0f) {
        return 0;
    }
    if (upper_value > -70.0f) {
        *low_threshold = UINT32_C(0xc28c0000);
        *high_threshold = UINT32_C(0xc2700000);
        return 1;
    }
    return 0;
}

static vf2_status camera_normalize_bounds(
    vf2_model2a *machine,
    uint32_t center,
    uint32_t *lower,
    uint32_t *upper
)
{
    uint16_t divisor_halfword = 0u;
    uint32_t divisor = 0u;
    vf2_status status = viewport_read_u16(
        machine, CAMERA_RANGE_DIVISOR, &divisor_halfword
    );
    if (status == VF2_OK) {
        divisor = (uint32_t)divisor_halfword;
        status = vf2_model2a_write_u32(
            machine, CAMERA_SCRATCH_ADDRESS, divisor
        );
    }
    if (status == VF2_OK) {
        *lower = viewport_div(viewport_sub(*lower, center), divisor);
        *upper = viewport_div(viewport_sub(*upper, center), divisor);
    }
    return status;
}

vf2_status vf2_recovered_task_camera_viewport_construct(
    vf2_model2a *machine,
    uint32_t registry_address,
    vf2_recovered_camera_viewport_report *report
)
{
    static const uint32_t first_fixed[8] = {
        UINT32_C(0x0000068e), UINT32_C(0x000006bf),
        UINT32_C(0x000006df), UINT32_C(0x000006bf),
        UINT32_C(0x000006df), UINT32_C(0x000006bf),
        UINT32_C(0x000006df), UINT32_C(0x0000068e)
    };
    static const uint32_t second_fixed[10] = {
        UINT32_C(0x00000765), UINT32_C(0x00000765),
        UINT32_C(0x00000765), UINT32_C(0x00000791),
        UINT32_C(0x00000791), UINT32_C(0x00000791),
        UINT32_C(0x00000791), UINT32_C(0x00000765),
        UINT32_C(0x00000765), UINT32_C(0x00000765)
    };
    vf2_recovered_camera_viewport_report local_report;
    vf2_recovered_camera_projection_report projection_report;
    uint8_t input_index = 0u;
    uint16_t input_flags = 0u;
    uint32_t scale = 0u;
    uint32_t lower = 0u;
    uint32_t upper = 0u;
    uint32_t center = 0u;
    uint32_t low_threshold = 0u;
    uint32_t high_threshold = 0u;
    uint32_t task_flags = 0u;
    size_t written = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL ||
        !viewport_registry_range_valid(registry_address, UINT32_C(0x178))) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(&local_report, 0, sizeof(local_report));
    memset(&projection_report, 0, sizeof(projection_report));
    local_report.start_address = UINT32_C(0x0001d678);
    local_report.stop_address = UINT32_C(0x0001d8e8);
    local_report.registry_address = registry_address;

    status = vf2_model2a_read(
        machine, CAMERA_INPUT_INDEX, &input_index, sizeof(input_index)
    );
    if (status == VF2_OK) {
        status = viewport_read_u16(
            machine, CAMERA_INPUT_TABLE + ((uint32_t)input_index << 8u),
            &input_flags
        );
    }
    if (status != VF2_OK) {
        return status;
    }
    if ((input_flags & (UINT16_C(1) << 3u)) == 0u) {
        return VF2_ERROR_UNSUPPORTED;
    }
    local_report.input_flags = input_flags;

    status = vf2_model2a_read_u32(machine, CAMERA_RANGE_SCALE, &scale);
    if (status == VF2_OK) {
        status = vf2_recovered_camera_range_window(
            machine, scale, &lower, &upper, &center
        );
    }
    if (status == VF2_OK) {
        local_report.first_lower = lower;
        local_report.first_upper = upper;
        local_report.first_center = center;
    }
    if (status == VF2_OK &&
        !camera_select_thresholds(
            lower, upper, &low_threshold, &high_threshold
        )) {
        status = camera_write_fixed_table(
            machine, registry_address + UINT32_C(0x100),
            first_fixed, sizeof(first_fixed) / sizeof(first_fixed[0])
        );
        if (status == VF2_OK) {
            local_report.first_fixed_table = 1;
            local_report.first_entries_written = 8u;
            local_report.task_bytes_written += sizeof(first_fixed);
        }
    } else if (status == VF2_OK) {
        status = vf2_recovered_camera_project_fighter_ranges(
            machine,
            viewport_sub(low_threshold, center),
            viewport_sub(high_threshold, center),
            &projection_report
        );
        if (status == VF2_OK &&
            viewport_bits_to_float(lower) < viewport_bits_to_float(low_threshold)) {
            lower = low_threshold;
        }
        if (status == VF2_OK &&
            viewport_bits_to_float(upper) > viewport_bits_to_float(high_threshold)) {
            upper = high_threshold;
        }
        if (status == VF2_OK) {
            status = camera_normalize_bounds(machine, center, &lower, &upper);
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, CAMERA_FIRST_NORMALIZED_LOW, lower
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_write_u32(
                machine, CAMERA_FIRST_NORMALIZED_HIGH, upper
            );
        }
        if (status == VF2_OK) {
            const uint32_t factor = viewport_div(UINT32_C(0x40c00000), scale);
            status = vf2_recovered_camera_fill_viewport_table(
                machine,
                viewport_mul(UINT32_C(0xc0be6666), factor),
                viewport_mul(UINT32_C(0xc099999a), factor),
                8u,
                viewport_mul(UINT32_C(0x3fcccccd), factor),
                registry_address + UINT32_C(0x100),
                lower,
                upper,
                &written
            );
        }
        if (status == VF2_OK) {
            status = vf2_model2a_read_u32(machine, registry_address, &task_flags);
        }
        if (status == VF2_OK) {
            task_flags |= UINT32_C(1);
            status = vf2_model2a_write_u32(machine, registry_address, task_flags);
        }
        if (status == VF2_OK) {
            local_report.first_entries_written = written;
            local_report.task_bytes_written += written * sizeof(uint32_t) +
                sizeof(uint32_t);
        }
    }

    if (status == VF2_OK) {
        status = vf2_recovered_camera_range_window(
            machine, UINT32_C(0x41300000), &lower, &upper, &center
        );
    }
    if (status == VF2_OK) {
        local_report.second_lower = lower;
        local_report.second_upper = upper;
        local_report.second_center = center;
    }
    if (status == VF2_OK &&
        !camera_select_thresholds(
            lower, upper, &low_threshold, &high_threshold
        )) {
        status = camera_write_fixed_table(
            machine, registry_address + UINT32_C(0x150),
            second_fixed, sizeof(second_fixed) / sizeof(second_fixed[0])
        );
        if (status == VF2_OK) {
            local_report.second_fixed_table = 1;
            local_report.second_entries_written = 10u;
            local_report.task_bytes_written += sizeof(second_fixed);
        }
    } else if (status == VF2_OK) {
        status = vf2_recovered_camera_project_fighter_ranges(
            machine,
            viewport_sub(low_threshold, center),
            viewport_sub(high_threshold, center),
            &projection_report
        );
        if (status == VF2_OK &&
            viewport_bits_to_float(lower) < viewport_bits_to_float(low_threshold)) {
            lower = low_threshold;
        }
        if (status == VF2_OK &&
            viewport_bits_to_float(upper) > viewport_bits_to_float(high_threshold)) {
            upper = high_threshold;
        }
        if (status == VF2_OK) {
            status = camera_normalize_bounds(machine, center, &lower, &upper);
        }
        written = 0u;
        if (status == VF2_OK) {
            status = vf2_recovered_camera_fill_viewport_table(
                machine,
                UINT32_C(0xc12f3333),
                UINT32_C(0xc10ccccd),
                10u,
                UINT32_C(0x400ccccd),
                registry_address + UINT32_C(0x150),
                lower,
                upper,
                &written
            );
        }
        if (status == VF2_OK) {
            local_report.second_entries_written = written;
            local_report.task_bytes_written += written * sizeof(uint32_t);
        }
    }

    if (status == VF2_OK) {
        local_report.global_bytes_written = 24u;
        local_report.copro_scratch_bytes_written = sizeof(uint32_t);
        local_report.helpers_recovered = 3u;
        if (report != NULL) {
            *report = local_report;
        }
    }
    return status;
}
