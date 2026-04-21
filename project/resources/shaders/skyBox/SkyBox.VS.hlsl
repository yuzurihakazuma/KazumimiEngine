#include "SkyBox.hlsli"

cbuffer Camera: register(b0){
    matrix view;
    matrix projection;
};

struct VertexShaderInput{
    float32_t4 position : POSITION;
    
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
 
    matrix viewNoTranslation = view;
    viewNoTranslation._41 = 0.0f;
    viewNoTranslation._42 = 0.0f;
    viewNoTranslation._43 = 0.0f;
    
    
    output.position = mul(input.position, mul(viewNoTranslation,projection));
    
    output.position.z = output.position.w;
    
    output.texcoord = input.position.xyz;
    
    return output;
    
    
}