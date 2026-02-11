#include "Object3d.hlsli"


ConstantBuffer<Material> gMaterial : register(b0);
Texture2D<float32_t4> gTextrue : register(t0);
SamplerState gSampler : register(s0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gcamera : register(b2);
struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    if (textureColor.a <= 0.5)
    {
        discard;
    }
    
    float32_t3 toEye = normalize(gcamera.worldPosition - input.worldPosition);
    
    if (gMaterial.enableLighting != 0) 
    {
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        
        
        float32_t3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        
        
        float32_t3 halfVector = normalize(-gDirectionalLight.direction + toEye);
        
        float NDotH = dot(normalize(input.normal), halfVector);
        
        float specularPow = pow(saturate(NDotH), gMaterial.shininess);
        
        float32_t3 specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);
        
        output.color.rgb = diffuse + specular;
        output.color.a = gMaterial.color.a * textureColor.a;
       
    }
    else
    {
       
        output.color = gMaterial.color * textureColor;
 
    }
   
    
    return output;
    
}