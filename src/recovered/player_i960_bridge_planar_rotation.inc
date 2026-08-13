#include <math.h>

/*
 * Clean-room semantic model of TGP command 0x2d805b5b.
 *
 * Internal ROM evidence fixes the orientation. At 0x3a320 the game obtains
 * theta = atan2(z1-z0, x1-x0), adds one quarter turn, and immediately rotates
 * the same X/Z geometry before taking axis-aligned extrema. The only planar
 * matrix that makes that edge transverse component vanish is the clockwise
 * convention below:
 *
 *   x' = x*cos(theta) + z*sin(theta)
 *   z' = z*cos(theta) - x*sin(theta)
 *
 * The later -theta calls in the same routine are therefore the exact inverse.
 *
 * Angles are 16-bit turns: 0x4000 == pi/2. The real TGP sources sine/cosine
 * from its lookup ROM. The native player runtime does not currently expose
 * those table ROMs through vf2_model2a, so this helper reconstructs the same
 * mathematical samples with sinf while preserving the hardware's integer
 * quadrant reduction and exact cardinal values. The V2.2 sine table differs
 * from correctly rounded host sine by one ULP at a small minority of samples;
 * that numerical boundary is explicit rather than hidden behind the flat
 * coprocessor-port fallback.
 */

static float player_planar_rotation_float(uint32_t bits)
{
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t player_planar_rotation_bits(float value)
{
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float player_planar_rotation_sine(uint16_t angle)
{
    const uint32_t raw = (uint32_t)angle;
    uint32_t index = raw & UINT32_C(0x3fff);
    float value = 0.0f;

    if ((raw & UINT32_C(0x4000)) != 0u) {
        const uint32_t mirrored = UINT32_C(0x4000) - index;
        index = mirrored < UINT32_C(0x3fff)
            ? mirrored : UINT32_C(0x3fff);
    }

    if (index == 0u) {
        value = 0.0f;
    } else if (index >= UINT32_C(0x3ffe)) {
        /* The recovered V2.2 TGP table stores exact 1.0 at both final samples. */
        value = 1.0f;
    } else {
        /* pi / 32768, rounded once to binary32. */
        value = sinf((float)index * 0x1.921fb6p-14f);
    }
    if ((raw & UINT32_C(0x8000)) != 0u) {
        value = -value;
    }
    return value;
}

static vf2_status player_planar_rotation_apply(
    int16_t angle,
    uint32_t x_bits,
    uint32_t z_bits,
    uint32_t *rotated_x_bits,
    uint32_t *rotated_z_bits
)
{
    const uint16_t raw_angle = (uint16_t)angle;
    const float x = player_planar_rotation_float(x_bits);
    const float z = player_planar_rotation_float(z_bits);
    const float sine = player_planar_rotation_sine(raw_angle);
    const float cosine = player_planar_rotation_sine(
        (uint16_t)(raw_angle + UINT16_C(0x4000))
    );
    float x_cos = 0.0f;
    float z_sin = 0.0f;
    float z_cos = 0.0f;
    float x_sin = 0.0f;
    float rotated_x = 0.0f;
    float rotated_z = 0.0f;

    if (rotated_x_bits == NULL || rotated_z_bits == NULL ||
        !isfinite(x) || !isfinite(z) ||
        !isfinite(sine) || !isfinite(cosine)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    /* Keep the four binary32 products explicit so normal builds do not turn
     * the recovered multiply/add sequence into a guessed fused operation. */
    x_cos = x * cosine;
    z_sin = z * sine;
    z_cos = z * cosine;
    x_sin = x * sine;
    rotated_x = x_cos + z_sin;
    rotated_z = z_cos - x_sin;
    if (!isfinite(rotated_x) || !isfinite(rotated_z)) {
        return VF2_ERROR_UNSUPPORTED;
    }

    *rotated_x_bits = player_planar_rotation_bits(rotated_x);
    *rotated_z_bits = player_planar_rotation_bits(rotated_z);
    return VF2_OK;
}
