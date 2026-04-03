


Texture2D<float4> gTexture : register(t0); // 元の画面のテクスチャ
SamplerState gSampler : register(s0);

cbuffer BloomData : register(b0)
{
    float threshold; // UIのスライダーの値
    float3 padding;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};


float4 main(VertexShaderOutput input) : SV_TARGET
{
    // マスク画像の色を取得（光らないオブジェクトは既に真っ黒になっています）
    float4 color = gTexture.Sample(gSampler, input.texcoord);
    
    // MRTのおかげで光るものしか描かれないため、単純な掛け算で綺麗に発光します
    return float4(color.rgb * threshold, color.a);
}