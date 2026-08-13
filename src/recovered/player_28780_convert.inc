#include <math.h>
#include <string.h>

static float player_28780_float_from_bits(uint32_t bits)
{
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static vf2_status player_28780_cvtri_low16(
    uint32_t arithmetic_control,
    uint32_t word,
    uint32_t *converted_word
)
{
    const float value = player_28780_float_from_bits(word);
    const unsigned mode =
        (unsigned)((arithmetic_control >> 30u) & UINT32_C(3));
    double rounded = 0.0;
    uint16_t converted = 0u;

    if (converted_word == NULL || !isfinite(value)) {
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
    converted = (uint16_t)(int32_t)rounded;
    *converted_word =
        (word & UINT32_C(0xffff0000)) | (uint32_t)converted;
    return VF2_OK;
}
