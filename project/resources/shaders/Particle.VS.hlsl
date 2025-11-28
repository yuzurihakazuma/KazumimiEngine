#include "Particle.hlsli"

struct ParticleForGPU
{
    float32_t4x4 WVP;
    float32_t4x4 World;
    float32_t4 color;
};


StructuredBuffer<ParticleForGPU> gParticle : register(t0);


struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal   : NORMAL0;
};


VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;

    ParticleForGPU tm = gParticle[instanceId];

    
    output.position = mul(input.position, tm.WVP);

   
    output.texcoord = input.texcoord;

    
    float32_t3x3 world3x3 = (float32_t3x3)tm.World;
    output.normal = normalize(mul(input.normal, world3x3));
    
    output.color = gParticle[instanceId].color;
    

    return output;
}
