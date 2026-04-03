#include "Particle.hlsli"
ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float32_t4> gTexture : register(t0); // 綴り(gTextrue)も念のため直しています
SamplerState gSampler : register(s0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

//  出力先を2枚に増やす
struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0; // 1枚目：通常の色
    float32_t4 mask : SV_TARGET1; // 2枚目：ブルームで光らせる用
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // textureのa値が0の時にPixelを棄却
    if (textureColor.a == 0.0)
    {
        discard;
    }
  
    // 1枚目に色を書き込む
    output.color = gMaterial.color * textureColor * input.color;
    
    //  2枚目にも同じ色を書き込む（これでブルームが反応します）
    output.mask = float4(output.color.rgb, 1.0f);
    
    return output;
}