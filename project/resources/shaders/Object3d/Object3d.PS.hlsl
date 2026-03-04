#include "Object3d.hlsli"

ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float4> gTexture : register(t0); 
SamplerState gSampler : register(s0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<PointLight> gPointLight : register(b3);
ConstantBuffer<SpotLight> gSpotLight : register(b4); 

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    if (textureColor.a <= 0.5)
    {
        discard;
    }
    
    float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
    
    // --- Œõ‚ÌŒvŽZŒ‹‰Ê‚ð“ü‚ê‚é•Ï” ---
    float3 diffuseDirectional = float3(0, 0, 0);
    float3 specularDirectional = float3(0, 0, 0);
    float3 diffusePoint = float3(0, 0, 0);
    float3 specularPoint = float3(0, 0, 0);
    float3 diffuseSpot = float3(0, 0, 0); 
    float3 specularSpot = float3(0, 0, 0);
    
    
    if (gMaterial.enableLighting != 0)
    {
        //  •½sŒõŒ¹ (Directional Light) ‚ÌŒvŽZ
        float NdotL_Dir = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos_Dir = pow(NdotL_Dir * 0.5f + 0.5f, 2.0f);
        diffuseDirectional = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos_Dir * gDirectionalLight.intensity;
        
        float3 halfVector_Dir = normalize(-gDirectionalLight.direction + toEye);
        float NDotH_Dir = dot(normalize(input.normal), halfVector_Dir);
        float specularPow_Dir = pow(saturate(NDotH_Dir), gMaterial.shininess);
        specularDirectional = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow_Dir * float3(1.0f, 1.0f, 1.0f);

        //  “_ŒõŒ¹ (Point Light) ‚ÌŒvŽZ
        float3 pointLightDirection = normalize(input.worldPosition - gPointLight.position);
        float NdotL_Point = dot(normalize(input.normal), -pointLightDirection);
        float cos_Point = pow(NdotL_Point * 0.5f + 0.5f, 2.0f);
        
        // ŒõŒ¹‚©‚çŒ»Ý‚ÌƒsƒNƒZƒ‹‚Ü‚Å‚Ì‹——£‚ð‘ª‚é
        float distance = length(gPointLight.position - input.worldPosition);
        // ‹——£‚ª radius ‚É‹ß‚Ã‚­‚Ù‚Ç 0 ‚É‚È‚é‚æ‚¤‚ÈŒW”(factor)‚ðì‚é
        float factor = pow(saturate(-distance / gPointLight.radius + 1.0f), gPointLight.decay);
        
        
        diffusePoint = gMaterial.color.rgb * textureColor.rgb * gPointLight.color.rgb * cos_Point * gPointLight.intensity * factor;
        
        
        float3 halfVector_Point = normalize(-pointLightDirection + toEye);
        float NDotH_Point = dot(normalize(input.normal), halfVector_Point);
        float specularPow_Point = pow(saturate(NDotH_Point), gMaterial.shininess);
        // ‹——£‚ª radius ‚É‹ß‚Ã‚­‚Ù‚Ç 0 ‚É‚È‚é‚æ‚¤‚ÈŒW”(factor)‚ðì‚é
        specularPoint = gPointLight.color.rgb * gPointLight.intensity * specularPow_Point * float3(1.0f, 1.0f, 1.0f) * factor;
        
        float3 spotLightDirectionOnSurface = normalize(input.worldPosition - gSpotLight.position);
        float cosAngle = dot(spotLightDirectionOnSurface, gSpotLight.direction);
        
        // ƒtƒH[ƒ‹ƒIƒt(Šp“x‚É‚æ‚éŒ¸Š)‚ÌŒvŽZ
        float falloffFactor = saturate((cosAngle - gSpotLight.cosAngle) / (gSpotLight.cosFalloffStart - gSpotLight.cosAngle));
        
        // ‹——£‚É‚æ‚éŒ¸Š‚ÌŒvŽZ
        float spotDistance = length(gSpotLight.position - input.worldPosition);
        float attenuationFactor = pow(saturate(-spotDistance / gSpotLight.distance + 1.0f), gSpotLight.decay);
        
        // Œõ‚Ì“–‚½‚è•ûiDiffuse / Specularj‚ÌŒvŽZ
        float NdotL_Spot = dot(normalize(input.normal), -spotLightDirectionOnSurface);
        float cos_Spot = pow(NdotL_Spot * 0.5f + 0.5f, 2.0f);
        
        diffuseSpot = gMaterial.color.rgb * textureColor.rgb * gSpotLight.color.rgb * cos_Spot * gSpotLight.intensity * attenuationFactor * falloffFactor;
        
        float3 halfVector_Spot = normalize(-spotLightDirectionOnSurface + toEye);
        float NDotH_Spot = dot(normalize(input.normal), halfVector_Spot);
        float specularPow_Spot = pow(saturate(NDotH_Spot), gMaterial.shininess);
        
        specularSpot = gSpotLight.color.rgb * gSpotLight.intensity * specularPow_Spot * float3(1.0f, 1.0f, 1.0f) * attenuationFactor * falloffFactor;
        
        
        // --- Œõ‚ÌŒvŽZŒ‹‰Ê‚ð‡¬ 
        output.color.rgb = diffuseDirectional + specularDirectional + diffusePoint + specularPoint + diffuseSpot + specularSpot;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }
    
    return output;
}