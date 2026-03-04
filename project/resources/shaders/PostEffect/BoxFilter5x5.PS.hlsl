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
    float32_t dx = 1.0f / (float32_t) width;
    float32_t dy = 1.0f / (float32_t) height;

    // =======================================================
    // 5x5のカーネル（重み付け配列）
    // 中心(16)が一番濃く、端(1)にいくほど薄く混ぜる設計図
    // =======================================================
    float32_t kernel[5][5] =
    {
        { 1.0f, 4.0f, 6.0f, 4.0f, 1.0f },
        { 4.0f, 16.0f, 24.0f, 16.0f, 4.0f },
        { 6.0f, 24.0f, 36.0f, 24.0f, 6.0f },
        { 4.0f, 16.0f, 24.0f, 16.0f, 4.0f },
        { 1.0f, 4.0f, 6.0f, 4.0f, 1.0f }
    };
    
    float32_t4 finalColor = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
    
    // 5x5マス（縦-2〜+2、横-2〜+2 の合計25ピクセル）を調べる
    for (int y = -2; y <= 2; ++y)
    {
        for (int x = -2; x <= 2; ++x)
        {
            float32_t2 offset = float32_t2(x * dx, y * dy);
            float32_t2 sampleUV = input.texcoord + offset;
            
            // カーネルから「混ぜる割合（重み）」を取り出す
            // 配列のインデックスは 0〜4 なので、xとyに +2 をして取り出します
            float32_t weight = kernel[y + 2][x + 2];
            
            // 色を取得して、重みを掛け算して箱に足し込む
            finalColor += gTexture.Sample(gSampler, sampleUV) * weight;
        }
    }
    
    // カーネルの合計値（256）で割って、最終的な色にする
    output.color.rgb = finalColor.rgb / 256.0f;
    output.color.a = 1.0f;
    
    return output;
}