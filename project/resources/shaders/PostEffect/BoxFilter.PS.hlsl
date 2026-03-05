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
   // テクスチャの幅と高さを取得
    uint width, height;
    gTexture.GetDimensions(width, height);
    // ピクセルのUV座標をずらす幅を計算
    float32_t dx = 1.0f / (float32_t) width;
    float32_t dy = 1.0f / (float32_t) height;
    
    // 3x3の箱を作るためのループ
    float32_t4 finalColor = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
    
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            // 調べたいピクセルのUV座標を計算（中心 ＋ ずらし幅）
            float32_t2 offset = float32_t2(x * dx, y * dy);
            float32_t2 sampleUV = input.texcoord + offset;
            
            // 色を取得して箱に足し込む
            finalColor += gTexture.Sample(gSampler, sampleUV);
        }
    }
    // 箱の合計を9で割る（平均を取る）
    output.color.rgb = finalColor.rgb / 9.0f;
    
    output.color.a = 1.0f;
    
        return output;
}