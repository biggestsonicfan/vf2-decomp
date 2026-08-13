#define VF2_PLAYER_28AF8_WORD_COUNT 60u

static uint32_t player_28af8_negate(uint32_t value)
{
    return UINT32_C(0) - value;
}

static void player_28af8_swap(uint32_t *first, uint32_t *second)
{
    const uint32_t value = *first;
    *first = *second;
    *second = value;
}

static void player_28af8_swap_negate(uint32_t *first, uint32_t *second)
{
    const uint32_t first_value = *first;
    const uint32_t second_value = *second;
    *first = player_28af8_negate(second_value);
    *second = player_28af8_negate(first_value);
}

static void player_28af8_swap_toggle_sign(uint32_t *first, uint32_t *second)
{
    const uint32_t first_value = *first ^ UINT32_C(0x80000000);
    const uint32_t second_value = *second ^ UINT32_C(0x80000000);
    *first = second_value;
    *second = first_value;
}

static void player_28af8_store_low16(uint32_t *word, uint16_t value)
{
    *word = (*word & UINT32_C(0xffff0000)) | (uint32_t)value;
}

static void player_28af8_negate_signed_low16(uint32_t *word)
{
    const int32_t signed_value = (int32_t)(int16_t)(uint16_t)*word;
    const uint16_t result =
        (uint16_t)(UINT32_C(0) - (uint32_t)signed_value);
    player_28af8_store_low16(word, result);
}

static void player_28af8_reflect_low16(uint32_t *word)
{
    const uint16_t value = (uint16_t)*word;
    player_28af8_store_low16(
        word, (uint16_t)(UINT16_C(0x8000) - value)
    );
}

static vf2_status player_28af8_transform(
    uint32_t words[VF2_PLAYER_28AF8_WORD_COUNT]
)
{
    if (words == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    player_28af8_negate_signed_low16(&words[0x34u / 4u]);
    player_28af8_negate_signed_low16(&words[0x38u / 4u]);
    words[0x90u / 4u] ^= UINT32_C(0x80000000);
    player_28af8_reflect_low16(&words[0x3cu / 4u]);
    player_28af8_reflect_low16(&words[0x44u / 4u]);

    player_28af8_swap_negate(&words[0x54u / 4u], &words[0x60u / 4u]);
    player_28af8_swap_negate(&words[0x58u / 4u], &words[0x64u / 4u]);
    player_28af8_swap(&words[0x5cu / 4u], &words[0x68u / 4u]);

    player_28af8_swap_toggle_sign(&words[0xb4u / 4u], &words[0xc0u / 4u]);
    player_28af8_swap(&words[0xb8u / 4u], &words[0xc4u / 4u]);
    player_28af8_swap(&words[0xbcu / 4u], &words[0xc8u / 4u]);

    player_28af8_swap_negate(&words[0x00u / 4u], &words[0x0cu / 4u]);
    player_28af8_swap_negate(&words[0x04u / 4u], &words[0x10u / 4u]);
    player_28af8_swap(&words[0x08u / 4u], &words[0x14u / 4u]);

    player_28af8_reflect_low16(&words[0x6cu / 4u]);
    player_28af8_reflect_low16(&words[0x74u / 4u]);
    player_28af8_swap_negate(&words[0x78u / 4u], &words[0x84u / 4u]);
    player_28af8_swap_negate(&words[0x7cu / 4u], &words[0x88u / 4u]);
    player_28af8_swap(&words[0x80u / 4u], &words[0x8cu / 4u]);

    player_28af8_swap_toggle_sign(&words[0xd8u / 4u], &words[0xe4u / 4u]);
    player_28af8_swap(&words[0xdcu / 4u], &words[0xe8u / 4u]);
    player_28af8_swap(&words[0xe0u / 4u], &words[0xecu / 4u]);

    player_28af8_swap(&words[0x18u / 4u], &words[0x24u / 4u]);
    player_28af8_swap_negate(&words[0x1cu / 4u], &words[0x28u / 4u]);
    player_28af8_swap_negate(&words[0x20u / 4u], &words[0x2cu / 4u]);
    return VF2_OK;
}
