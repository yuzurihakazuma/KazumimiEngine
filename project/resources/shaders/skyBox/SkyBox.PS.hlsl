#include "SkyBox.hlsli"

TextureCube<float32_t4> skyboxTexture : register(t0);
SamplerState smp : register(s0);

// The scene renders into two targets (color + glow mask). The sky never glows.
struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
    float32_t4 mask : SV_TARGET1;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    output.color = skyboxTexture.Sample(smp, input.texcoord);
    output.mask = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
    return output;
}
