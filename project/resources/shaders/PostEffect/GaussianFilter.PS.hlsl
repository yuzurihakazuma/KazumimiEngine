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
    uint width, height;
    gTexture.GetDimensions(width, height);
    // ピクセルサイズを計算
    float32_t dx = 1.0f / (float32_t) width;
    float32_t dy = 1.0f / (float32_t) height;
    
    // ガウシアンカーネルの重み
    float32_t4 finalColor = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
    
    float32_t sigma = 2.0f; // ガウシアンの標準偏差
    // ガウシアンカーネルのサイズ
    float32_t totalWeight = 0.0f;
    
    // 5x5マス（半径2）を調べるループ
    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            float32_t2 offset = float32_t2(x * dx, y * dy);
            float32_t2 sampleUV = input.texcoord + offset;
            
            float32_t weight = exp(-(x * x + y * y) / (2.0f * sigma * sigma));
            
            // 色に重みを掛けて足し込む
            finalColor += gTexture.Sample(gSampler, sampleUV) * weight;
            
            // 後で合計を1.0にするため、足した重みを記憶しておく
            totalWeight += weight;
        }
    }
    
    output.color.rgb = finalColor.rgb / totalWeight; // 重みで割って正規化
    
    output.color.a = 1.0f;
    
    return output;
}