#include "Fullscreen.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);


// C++から「時間」を受け取る受け口(b0)
cbuffer TimeBuffer : register(b0)
{
    float32_t time;
};

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

// UV座標からランダムな数値(0.0 ～ 1.0)を生み出す魔法の関数
float32_t Random(float32_t2 uv)
{
    return frac(sin(dot(uv, float32_t2(12.9898f, 78.233f))) * 43758.5453123f);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // 1. 元のゲーム画面の色を取得
    float32_t4 originalColor = gTexture.Sample(gSampler, input.texcoord);
    
    // 2. ピクセルの座標(UV)を使ってランダムな値を作る
    float32_t noise = Random(input.texcoord + float32_t2(time, time));
    

    // 3. ノイズを白黒の色(float3)にする
    float32_t3 noiseColor = float32_t3(noise, noise, noise);
    
    // 4. 元の画面(80%) と ノイズ(20%) を混ぜ合わせる！
    // ※ 0.2f を大きくするとノイズが濃くなります
    output.color.rgb = originalColor.rgb * 0.8f + noiseColor * 0.2f;
    output.color.a = originalColor.a;
    
    return output;
}