typedef float4 float32_t4;
typedef float3 float32_t3;
typedef float2 float32_t2;
typedef float4x4 float32_t4x4;


struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
};

struct Material
{
    float4 color;
    int32_t enableLighting;
    float4x4 uvTransform;
    float shininess;
    float2 padding;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

struct Camera
{
    float3 worldPosition;
};