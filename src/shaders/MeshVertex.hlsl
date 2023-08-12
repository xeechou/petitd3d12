struct UniformData
{
	float4x4 MVP;
	float4x4 NormalMatrix;
	float4x4 ModelMatrix;
	float4   eye;
};

ConstantBuffer<UniformData> Uniform : register(b0);

struct VertexPos
{
	float3 Position : POSITION;
	float3 Normal   : NORMAL;
	float3 texcoord : TEXCOORD;
};

struct VertexShaderOutput
{
	float3 Normal   : NORMAL;
	float3 WPos     : POSITION;
	float4 Position : SV_Position;
};

VertexShaderOutput main(VertexPos IN)
{
    VertexShaderOutput OUT;

    OUT.Position = mul(Uniform.MVP, float4(IN.Position, 1.0f));
    float3 Normal = mul((float3x3)Uniform.NormalMatrix, IN.Normal);
    OUT.Normal = normalize(Normal);
    OUT.WPos   = (float3)mul(Uniform.ModelMatrix, float4(IN.Position, 1.0f));
    // OUT.Color = float4(Normal, 1.0);
    // OUT.Color = float4(depth, depth, depth, 1.0);

    return OUT;
}
