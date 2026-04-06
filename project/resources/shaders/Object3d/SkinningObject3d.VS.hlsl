#include "Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

// MatrixPalette：全ジョイントの最終変換行列の配列
StructuredBuffer<float4x4> gMatrixPalette : register(t2);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 weight : WEIGHT0; // VertexInfluence の weights[4]
    int4 index : INDEX0; // VertexInfluence の jointIndices[4]
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    // 4本のジョイント行列を重みで合成して1つのスキン行列を作る
    float4x4 skinMatrix =
        input.weight.x * gMatrixPalette[input.index.x] +
        input.weight.y * gMatrixPalette[input.index.y] +
        input.weight.z * gMatrixPalette[input.index.z] +
        input.weight.w * gMatrixPalette[input.index.w];

    // スキニング済みの座標・法線を計算
    float4 skinnedPosition = mul(input.position, skinMatrix);
    float3 skinnedNormal = normalize(mul(input.normal, (float3x3) skinMatrix));

    // あとは通常の Object3D と同じ
    output.position = mul(skinnedPosition, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(skinnedNormal, (float3x3) gTransformationMatrix.WorldInverseTranspose));
    output.worldPosition = mul(skinnedPosition, gTransformationMatrix.World).xyz;

    return output;
}
