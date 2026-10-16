// VF2 model viewer -- fragment shader (HLSL, compiled to SPIR-V with dxc).
// SDL_GPU SPIR-V binding convention: fragment sampled textures/samplers in set 2.
// The texture is a single-channel (R8) luma-index sheet scaled to grayscale; the
// OBJ carries V with the usual up origin, so flip V for the top-origin GPU texture.
Texture2D<float4> tex : register(t0, space2);
SamplerState smp : register(s0, space2);

struct PSInput
{
    float4 pos : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    float g = tex.Sample(smp, float2(input.uv.x, 1.0f - input.uv.y)).r;
    return float4(g, g, g, 1.0f);
}
