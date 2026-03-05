#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{ 
    // テクスチャの色を取得
    PixelShaderOutput output;
    // グレースケールの値を計算
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
        // グレースケールの値を計算（輝度の重み付け平均）
    float32_t value = dot(textureColor.rgb, float32_t3(0.2125f, 0.7154f, 0.0721f));
    // RGB全てに同じグレースケールの値を設定し、アルファは元のテクスチャのアルファを使用
    output.color = float32_t4(value, value, value, textureColor.a);
    // 結果を返す
    return output;
}