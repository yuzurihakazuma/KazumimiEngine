#include "Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

// 行列の構造体のバッファ
StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t0);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceID : SV_InstanceID)
{
    VertexShaderOutput output;
    
    // 一度 tm という変数にコピーしてスッキリさせる！
    TransformationMatrix tm = gTransformationMatrices[instanceID];
    
    // 短くなった tm を使って計算！
    output.position = mul(input.position, tm.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3) tm.WorldInverseTranspose));
    output.worldPosition = mul(input.position, tm.World).xyz;
    
    return output;
}