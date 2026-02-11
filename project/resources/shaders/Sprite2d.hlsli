struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

struct Material
{
    float4 color;
    int32_t enableLighting;
    float4x4 uvTransform;
    float shininess;
};