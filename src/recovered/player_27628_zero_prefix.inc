static size_t player_27628_zero_prefix_count(uint32_t state, uint32_t c30)
{
    return ((state & (UINT32_C(1) << 20u)) != 0u ||
            (c30 & (UINT32_C(1) << 6u)) != 0u)
        ? 5u : 0u;
}
