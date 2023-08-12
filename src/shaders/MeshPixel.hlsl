struct PixelShaderInput
{
	float3 Normal    : NORMAL;
	float3 WPos      : POSITION;
};

struct UniformData
{
	float4x4 MVP;
	float4x4 NormalMatrix;
	float4x4 ModelMatrix;
	float4   eye;
};

struct Material
{
	float4 diffuse;  //w alpha
	float4 specular; //w shininess
	float4 light_dir;
};

ConstantBuffer<UniformData> uniform_data : register(b0);
ConstantBuffer<Material> material : register(b1);

float4 main( PixelShaderInput IN ) : SV_Target
{

	float3 albedo = material.diffuse.xyz;
	float3 specular = material.specular.xyz;
	float shininess = material.specular.w;
	float alpha = material.diffuse.w;
	//diffuse
	float NdotL = max(0.0f, dot(IN.Normal, material.light_dir.xyz));
	float3 diffuse = albedo * NdotL; //light color 1.0;
	//specular
	float3 viewdir = normalize((float3)uniform_data.eye - IN.WPos);
	float3 reflect_dir = reflect((float3)material.light_dir, IN.Normal);
	float spec = pow(max(dot(viewdir, reflect_dir), 0.0), shininess);

	return float4(albedo * (NdotL+spec), alpha); // IN.Color;
}
