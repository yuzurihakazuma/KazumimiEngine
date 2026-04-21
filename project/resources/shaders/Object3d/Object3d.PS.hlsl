#include "Object3d.hlsli"

ConstantBuffer<Material> gMaterial : register(b0); // マテリアル
Texture2D<float4> gTexture : register(t0); // 通常テクスチャ
SamplerState gSampler : register(s0); // サンプラー
ConstantBuffer<DirectionalLightData> gDirectionalLightData : register(b1);
ConstantBuffer<Camera> gCamera : register(b2); // カメラの定数バッファ
ConstantBuffer<PointLight> gPointLight : register(b3); // 点光源の定数バッファ
ConstantBuffer<SpotLight> gSpotLight : register(b4); // スポットライトの定数バッファ

Texture2D<float4> gNoiseTexture : register(t1); // ノイズテクスチャ
ConstantBuffer<DissolveData> gDissolve : register(b5); // ディゾルブ用

struct PixelShaderOutput
{
    float4 color : SV_TARGET0; // 通常カラー
    float4 mask : SV_TARGET1; // Bloom用マスク
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    float innerGlow = 0.0f; // 内側の強い光
    float outerGlow = 0.0f; // 外側の弱い光

    // =========================================================
    // ディゾルブ境界の計算
    // =========================================================
    if (gDissolve.threshold > 0.0f)
    {
        float noiseValue = gNoiseTexture.Sample(gSampler, transformedUV.xy).r;

        float innerWidth = 0.04f; // 内側の細い発光帯
        float outerWidth = 0.16f; // 外側の広い発光帯

        // 閾値より内側は消す
        if (noiseValue <= gDissolve.threshold)
        {
            discard;
        }

        // 外側の柔らかい発光
        if (noiseValue <= gDissolve.threshold + outerWidth)
        {
            outerGlow = 1.0f - saturate((noiseValue - gDissolve.threshold) / outerWidth);
        }

        // 内側の強い発光
        if (noiseValue <= gDissolve.threshold + innerWidth)
        {
            innerGlow = 1.0f - saturate((noiseValue - gDissolve.threshold) / innerWidth);
        }
    }

    // =========================================================
    // ライティング計算
    // =========================================================
    float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);

    float3 diffuseDirectional = float3(0, 0, 0);
    float3 specularDirectional = float3(0, 0, 0);
    float3 diffusePoint = float3(0, 0, 0);
    float3 specularPoint = float3(0, 0, 0);
    float3 diffuseSpot = float3(0, 0, 0);
    float3 specularSpot = float3(0, 0, 0);

    if (gMaterial.enableLighting != 0)
    {
        for (int i = 0; i < gDirectionalLightData.activeCount; ++i)
        {
            float NdotL_Dir = dot(normalize(input.normal), -gDirectionalLightData.lights[i].direction);
            float cos_Dir = pow(NdotL_Dir * 0.5f + 0.5f, 2.0f);
            
            // += を使って光を足し合わせる
            diffuseDirectional += gMaterial.color.rgb * textureColor.rgb * gDirectionalLightData.lights[i].color.rgb * cos_Dir * gDirectionalLightData.lights[i].intensity;
            
            float3 halfVector_Dir = normalize(-gDirectionalLightData.lights[i].direction + toEye);
            float NDotH_Dir = dot(normalize(input.normal), halfVector_Dir);
            float specularPow_Dir = pow(saturate(NDotH_Dir), gMaterial.shininess);
            
            // += を使ってハイライトを足し合わせる
            specularDirectional += gDirectionalLightData.lights[i].color.rgb * gDirectionalLightData.lights[i].intensity * specularPow_Dir * float3(1.0f, 1.0f, 1.0f);
        }
        //  点光源 (Point Light) の計算
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
        
        
        float3 ambient = gMaterial.color.rgb * textureColor.rgb * 0.15f; // 0.15は環境光の強さ。お好みで調整してください
        output.color.rgb = ambient + diffuseDirectional + specularDirectional + diffusePoint + specularPoint + diffuseSpot + specularSpot;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }

// =========================================================
// ディゾルブ境界の見た目を通常カラー側にも少し足す
// =========================================================
    float3 edgeColor = gDissolve.edgeColor;

// 本体への加算はかなり弱め
    output.color.rgb += edgeColor * innerGlow * 0.1f;
    output.color.rgb += edgeColor * outerGlow * 0.03f;

// Bloom側はしっかり
    float glow = saturate(innerGlow * 1.0f + outerGlow * 0.7f);

    if (gDissolve.threshold > 0.0f && glow > 0.001f)
    {
        float bloomPower = 2.5f;
        output.mask = float4(edgeColor * glow * bloomPower, 1.0f);
    }
    else if (gMaterial.emissive > 1.0f)
    {
        output.mask = float4(output.color.rgb * gMaterial.emissive, 1.0f);
    }
    else
    {
        output.mask = float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    return output;
}