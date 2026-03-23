


Texture2D<float4> gTexture : register(t0); // 元の画面のテクスチャ
SamplerState gSampler : register(s0);

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

// 光らせる基準値（これより明るい色だけを抽出する）
// ※後でC++から操作できるようにしますが、今は固定値にしておきます
static const float threshold = 1.0f;

float4 main(VertexShaderOutput input) : SV_TARGET
{
    // 元の画面の色を取得
    float4 color = gTexture.Sample(gSampler, input.texcoord);
    
    // ピクセルの明るさ（輝度：Luminance）を計算する（人間の目に合わせた比率）
    float luminance = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    
    // 明るさが閾値(threshold)を超えていればその色を残し、超えていなければ真っ黒(0)にする
    float extractStrength = step(threshold, luminance);
    
    return float4(color.rgb * extractStrength, color.a);
}