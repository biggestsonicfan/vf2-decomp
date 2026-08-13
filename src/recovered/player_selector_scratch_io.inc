static vf2_status player_selector_scratch_read_u8(
    const vf2_model2a *machine,
    uint32_t address,
    uint8_t *value
)
{
    return value == NULL
        ? VF2_ERROR_INVALID_ARGUMENT
        : vf2_model2a_read(machine, address, value, sizeof(*value));
}

static vf2_status player_selector_scratch_read_u16(
    const vf2_model2a *machine,
    uint32_t address,
    uint16_t *value
)
{
    uint8_t raw[2] = {0u, 0u};
    vf2_status status = VF2_OK;

    if (value == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    status = vf2_model2a_read(machine, address, raw, sizeof(raw));
    if (status == VF2_OK) {
        *value = (uint16_t)((uint16_t)raw[0] |
                            ((uint16_t)raw[1] << 8u));
    }
    return status;
}

static vf2_status player_selector_scratch_write_u8(
    vf2_model2a *machine,
    uint32_t address,
    uint8_t value
)
{
    return vf2_model2a_write(machine, address, &value, sizeof(value));
}

static vf2_status player_selector_scratch_write_u16(
    vf2_model2a *machine,
    uint32_t address,
    uint16_t value
)
{
    const uint8_t raw[2] = {
        (uint8_t)value,
        (uint8_t)(value >> 8u)
    };
    return vf2_model2a_write(machine, address, raw, sizeof(raw));
}

static float player_selector_scratch_float(uint32_t bits)
{
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static vf2_status player_selector_scratch_cvtri(
    uint32_t arithmetic_control,
    uint32_t bits,
    uint16_t *result
)
{
    const float value = player_selector_scratch_float(bits);
    const unsigned mode =
        (unsigned)((arithmetic_control >> 30u) & UINT32_C(3));
    double rounded = 0.0;

    if (result == NULL || !isfinite(value)) {
        return VF2_ERROR_UNSUPPORTED;
    }
    switch (mode) {
    case 0u: {
        const double lower = floor((double)value);
        const double fraction = (double)value - lower;
        if (fraction < 0.5) {
            rounded = lower;
        } else if (fraction > 0.5) {
            rounded = lower + 1.0;
        } else {
            rounded = fmod(fabs(lower), 2.0) == 0.0
                ? lower : lower + 1.0;
        }
        break;
    }
    case 1u:
        rounded = floor((double)value);
        break;
    case 2u:
        rounded = ceil((double)value);
        break;
    case 3u:
    default:
        rounded = trunc((double)value);
        break;
    }
    if (rounded < (double)INT32_MIN || rounded > (double)INT32_MAX) {
        return VF2_ERROR_UNSUPPORTED;
    }
    *result = (uint16_t)(int32_t)rounded;
    return VF2_OK;
}

static vf2_status player_selector_scratch_add(
    uint32_t base,
    uint32_t amount,
    uint32_t *result
)
{
    if (result == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (base > UINT32_MAX - amount) {
        return VF2_ERROR_UNSUPPORTED;
    }
    *result = base + amount;
    return VF2_OK;
}

static vf2_status player_selector_scratch_advance(
    uint32_t *cursor,
    uint32_t count,
    uint32_t stride
)
{
    const uint64_t amount = (uint64_t)count * (uint64_t)stride;

    if (cursor == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (amount > UINT32_MAX ||
        *cursor > UINT32_MAX - (uint32_t)amount) {
        return VF2_ERROR_UNSUPPORTED;
    }
    *cursor += (uint32_t)amount;
    return VF2_OK;
}

static vf2_status player_selector_scratch_validate_write(
    const vf2_model2a *machine,
    uint32_t address,
    size_t size
)
{
    uint8_t byte = 0u;
    size_t index = 0u;
    vf2_status status = VF2_OK;

    if (machine == NULL ||
        (size > 0u && (uint64_t)address + (uint64_t)size - 1u > UINT32_MAX)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0u; status == VF2_OK && index < size; ++index) {
        status = vf2_model2a_read(
            machine, address + (uint32_t)index, &byte, sizeof(byte)
        );
    }
    return status;
}

static vf2_status player_selector_scratch_push_operation(
    vf2_player_selector_scratch_plan *plan,
    vf2_player_selector_scratch_operation **operation
)
{
    if (plan == NULL || operation == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    if (plan->operation_count >=
        VF2_PLAYER_SELECTOR_SCRATCH_MAX_OPERATIONS) {
        return VF2_ERROR_UNSUPPORTED;
    }
    *operation = &plan->operations[plan->operation_count++];
    memset(*operation, 0, sizeof(**operation));
    return VF2_OK;
}
