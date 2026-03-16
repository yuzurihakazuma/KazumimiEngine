#include "Object3d.hlsli"

ConstantBuffer<Material> gMaterial : register(b0); // マテリアル
Texture2D<float4> gTexture : register(t0); // 通常テクスチャ
SamplerState gSampler : register(s0); // サンプラー

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1); // 平行光源
ConstantBuffer<Camera> gCamera : register(b2); // カメラ
ConstantBuffer<PointLight> gPointLight : register(b3); // 点光源
ConstantBuffer<SpotLight> gSpotLight : register(b4); // スポットライト

Texture2D<float4> gNoiseTexture : register(t1); // ノイズテクスチャ
ConstantBuffer<DissolveData> gDissolve : register(b5); // ディゾルブ用

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    float innerGlow = 0.0f; // 内側の強い光
    float outerGlow = 0.0f; // 外側の弱い光

    if (gDissolve.threshold > 0.0f)
    {
        float noiseValue = gNoiseTexture.Sample(gSampler, transformedUV.xy).r;

        float innerWidth = 0.04f; // 内側の細い発光帯
        float outerWidth = 0.16f; // 外側の広い発光帯

        if (noiseValue <= gDissolve.threshold)
        {
            discard; // 消える部分
        }

        if (noiseValue <= gDissolve.threshold + outerWidth)
        {
            outerGlow = 1.0f - saturate((noiseValue - gDissolve.threshold) / outerWidth);
        }

        if (noiseValue <= gDissolve.threshold + innerWidth)
        {
            innerGlow = 1.0f - saturate((noiseValue - gDissolve.threshold) / innerWidth);
        }
    }

    float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

    float3 diffuseDirectional = float3(0, 0, 0);
    float3 specularDirectional = float3(0, 0, 0);
    float3 diffusePoint = float3(0, 0, 0);
    float3 specularPoint = float3(0, 0, 0);
    float3 diffuseSpot = float3(0, 0, 0);
    float3 specularSpot = float3(0, 0, 0);

    if (gMaterial.enableLighting != 0)
    {
        float NdotL_Dir = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos_Dir = pow(NdotL_Dir * 0.5f + 0.5f, 2.0f);
        diffuseDirectional = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos_Dir * gDirectionalLight.intensity;

        float3 halfVector_Dir = normalize(-gDirectionalLight.direction + toEye);
        float NDotH_Dir = dot(normalize(input.normal), halfVector_Dir);
        float specularPow_Dir = pow(saturate(NDotH_Dir), gMaterial.shininess);
        specularDirectional = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow_Dir * float3(1.0f, 1.0f, 1.0f);

        float3 pointLightDirection = normalize(input.worldPosition - gPointLight.position);
        float NdotL_Point = dot(normalize(input.normal), -pointLightDirection);
        float cos_Point = pow(NdotL_Point * 0.5f + 0.5f, 2.0f);

        float distance = length(gPointLight.position - input.worldPosition);
        float factor = pow(saturate(-distance / gPointLight.radius + 1.0f), gPointLight.decay);

        diffusePoint = gMaterial.color.rgb * textureColor.rgb * gPointLight.color.rgb * cos_Point * gPointLight.intensity * factor;

        float3 halfVector_Point = normalize(-pointLightDirection + toEye);
        float NDotH_Point = dot(normalize(input.normal), halfVector_Point);
        float specularPow_Point = pow(saturate(NDotH_Point), gMaterial.shininess);
        specularPoint = gPointLight.color.rgb * gPointLight.intensity * specularPow_Point * float3(1.0f, 1.0f, 1.0f) * factor;

        float3 spotLightDirectionOnSurface = normalize(input.worldPosition - gSpotLight.position);
        float cosAngle = dot(spotLightDirectionOnSurface, gSpotLight.direction);

        float falloffFactor = saturate((cosAngle - gSpotLight.cosAngle) / (gSpotLight.cosFalloffStart - gSpotLight.cosAngle));

        float spotDistance = length(gSpotLight.position - input.worldPosition);
        float attenuationFactor = pow(saturate(-spotDistance / gSpotLight.distance + 1.0f), gSpotLight.decay);

        float NdotL_Spot = dot(normalize(input.normal), -spotLightDirectionOnSurface);
        float cos_Spot = pow(NdotL_Spot * 0.5f + 0.5f, 2.0f);

        diffuseSpot = gMaterial.color.rgb * textureColor.rgb * gSpotLight.color.rgb * cos_Spot * gSpotLight.intensity * attenuationFactor * falloffFactor;

        float3 halfVector_Spot = normalize(-spotLightDirectionOnSurface + toEye);
        float NDotH_Spot = dot(normalize(input.normal), halfVector_Spot);
        float specularPow_Spot = pow(saturate(NDotH_Spot), gMaterial.shininess);

        specularSpot = gSpotLight.color.rgb * gSpotLight.intensity * specularPow_Spot * float3(1.0f, 1.0f, 1.0f) * attenuationFactor * falloffFactor;

        output.color.rgb = diffuseDirectional + specularDirectional + diffusePoint + specularPoint + diffuseSpot + specularSpot;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }

    float3 outerColor = float3(0.2f, 0.8f, 1.0f); // 外側の青
    float3 innerColor = float3(1.0f, 1.0f, 1.0f); // 内側の白

    // 外側に薄い色を混ぜる
    output.color.rgb = lerp(output.color.rgb, outerColor, outerGlow * 0.45f);

    // 内側に強い白を混ぜる
    output.color.rgb = lerp(output.color.rgb, innerColor, innerGlow * 0.75f);

    return output;
}