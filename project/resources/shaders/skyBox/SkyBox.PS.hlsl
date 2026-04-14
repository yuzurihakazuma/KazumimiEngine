#include "SkyBox.PS.hlsl"


TextureCube<float32_t4> skyboxTexture : register(t0);
SamplerState smp : register(s0);

float32_t4 main(VertexShaderOutput input) : SV_TARGET
{
    float32_t4 color = skyboxTexture.Sample(smp, input.texcoord);
    return color;
}