#include "../Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// C++から送られてくる「ぼかしの設定データ」
cbuffer BlurData : register(b0)
{
    float2 texelSize; // 1ピクセルの大きさ (例: 1.0/1280, 1.0/720)
    float2 direction; // ぼかす方向 (横なら[1,0]、縦なら[0,1])
}

// ガウスぼかしの重み（中心が一番濃く、端に行くほど薄くなる）
static const float weights[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // 中心のピクセルの色
    float32_t4 color = gTexture.Sample(gSampler, input.texcoord) * weights[0];
    
    // 中心から両側に4ピクセルずつ、色をサンプリングして足し合わせる
    for (int i = 1; i < 5; ++i)
    {
        float2 offset = direction * texelSize * (float) i;
        color += gTexture.Sample(gSampler, input.texcoord + offset) * weights[i];
        color += gTexture.Sample(gSampler, input.texcoord - offset) * weights[i];
    }
    
    output.color = color;
    return output;
}