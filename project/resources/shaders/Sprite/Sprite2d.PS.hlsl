#include "Sprite2d.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// How soft the leading edge of the reveal is (in "stroke order" units, 0..1).
static const float kRevealSoftness = 0.02f;

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    if (gMaterial.reveal >= 0.0f)
    {
        // Stroke-order image (same idea as the dissolve): R = shade, G = shape (coverage), B = order.
        // Parts drawn earlier have a larger B. Only the parts whose order has reached the threshold are shown.
        float drawn = smoothstep(gMaterial.reveal - kRevealSoftness, gMaterial.reveal + kRevealSoftness, textureColor.b);
        float alpha = gMaterial.color.a * textureColor.g * drawn;
        if (alpha <= 0.0f)
        {
            discard;
        }
        output.color = float4(gMaterial.color.rgb * textureColor.r, alpha);
        return output;
    }

    if (textureColor.a <= 0.0f)
    {
        discard;
    }

    output.color = gMaterial.color * textureColor;

    return output;
}