#include "Particle.hlsli"

struct GPUParticleData
{
    float3 position;
    float lifeTime;
    float3 velocity;
    float currentTime;
    float4 color;
    float scale;
    uint alive;
    float2 pad;
};

// t1: GPU側のパーティクルデータ (SRVとして読み込む)
StructuredBuffer<GPUParticleData> gParticles : register(t1);

// b1: カメラ行列 (ビルボード計算に使う)
cbuffer CameraMatrix : register(b1)
{
    float4x4 view;
    float4x4 projection;
};

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;

    GPUParticleData p = gParticles[instanceId];

    // 死亡パーティクルはクリップ空間の外へ飛ばす
    if (p.alive == 0)
    {
        output.position = float4(0, 0, -1, 0);
        output.texcoord = float2(0, 0);
        output.normal = float3(0, 0, 1);
        output.color = float4(0, 0, 0, 0);
        return output;
    }

    // ビルボード: ViewMatrixの右ベクトルと上ベクトルを取り出す
    float3 right = float3(view[0][0], view[1][0], view[2][0]);
    float3 up = float3(view[0][1], view[1][1], view[2][1]);

    // ビルボード展開した頂点のワールド座標
    float t = p.currentTime / p.lifeTime;
    float scale = p.scale * (1.0f - t * 0.5f); // 少し縮みながら消える
    float3 worldPos = p.position
                    + right * input.position.x * scale
                    + up * input.position.y * scale;

    // VP変換
    float4x4 vp = mul(view, projection);
    output.position = mul(float4(worldPos, 1.0f), vp);

    output.texcoord = input.texcoord;
    output.normal = float3(0, 0, -1);
    output.color = p.color;

    return output;
}
