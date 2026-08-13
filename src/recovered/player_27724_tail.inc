static float player_27724_float(uint32_t bits)
{
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t player_27724_bits(float value)
{
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t player_27724_scale_bits(uint32_t divisor_bits)
{
    const float divisor = player_27724_float(divisor_bits);
    return player_27724_bits(1.0f / divisor);
}

static vf2_status player_27734_build_delta_words(
    const uint32_t active[24],
    const uint32_t backup[24],
    uint32_t scale_bits,
    uint32_t delta[24]
)
{
    const float scale = player_27724_float(scale_bits);
    size_t index = 0u;

    if (active == NULL || backup == NULL || delta == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    for (index = 0u; index < 24u; ++index) {
        const float active_value = player_27724_float(active[index]);
        const float backup_value = player_27724_float(backup[index]);
        delta[index] = player_27724_bits(
            (backup_value - active_value) * scale
        );
    }
    return VF2_OK;
}
