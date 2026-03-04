#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
   
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    float32_t2 center = float32_t2(0.5f, 0.5f);
    
    float32_t dist = distance(input.texcoord, center);
    // smoothstep(最小値, 最大値, 測った距離) は、距離が0.3〜0.7の間に 0.0〜1.0 のグラデーションを作ってくれます。
    // それを 1.0 から引くことで、「中心は1.0（明るい）、端は0.0（暗い）」という係数にします。
    float32_t vignette = 1.0f - smoothstep(0.3f, 0.7f, dist);
    
    output.color.rgb = textureColor.rgb * vignette;
    
    output.color.a = textureColor.a;
    
    return output;
}