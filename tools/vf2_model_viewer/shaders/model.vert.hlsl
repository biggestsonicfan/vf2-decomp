// VF2 model viewer -- vertex shader (HLSL, compiled to SPIR-V with dxc).
// SDL_GPU SPIR-V binding convention: vertex uniform buffers live in set 1.
cbuffer UBO : register(b0, space1)
{
    float4x4 mvp;
};

struct VSInput
{
    [[vk::location(0)]] float3 pos : POSITION;
    [[vk::location(1)]] float2 uv  : TEXCOORD0;
};

struct VSOutput
{
    float4 pos : SV_Position;
    [[vk::location(0)]] float2 uv : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput o;
    o.pos = mul(mvp, float4(input.pos, 1.0f));
    o.uv = input.uv;
    return o;
}
