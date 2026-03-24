#include "../Fullscreen.hlsli"

// C++から送られてくる2枚の画像を受け取る！
Texture2D<float32_t4> gTextureMain : register(t0); // 元の画面 (PostEffectの結果)
Texture2D<float32_t4> gTextureBlur : register(t1); // ぼかした光 (Bloomの結果)
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // 同じ座標のピクセルの色を、2枚の画像からそれぞれ取得する
    float32_t4 mainColor = gTextureMain.Sample(gSampler, input.texcoord);
    float32_t4 blurColor = gTextureBlur.Sample(gSampler, input.texcoord);
    
    // 足し算するだけ！（これが加算合成の正体です）
    output.color = mainColor + blurColor;
    
    return output;
}