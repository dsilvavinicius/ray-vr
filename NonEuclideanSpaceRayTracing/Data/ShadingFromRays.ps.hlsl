__import ShaderCommon;
//__import Shading;
__import DefaultVS;

float4 calcBlinnPhongLighting(float3 N, float3 L, float3 H)
{
	float4 ambient = float4(0.25f, 0.20725f, 0.20725f, 1.0f);
	float4 diffuse = float4(1.0f, 0.829f, 0.829f, 1.0f);
	float4 specular = float4(0.296648f, 0.296648f, 0.296648f, 1.0f);
	float shininess = 11.264;

	float4 Id = diffuse * saturate(dot(N, L));
	float4 Is =  * pow(saturate(dot(N, H)), shininess);

	return ambient + Id + Is;
}

float4 main(VertexOut vOut) : SV_TARGET
{
	float3 V = -vOut.prevPosH.xyz;

	float4 finalColor = float4(0, 0, 0, 1);

	for (uint l = 0; l < gLightsCount; l++)
	{
		LightData light = gLights[l];
		H = normalize(-light.dirW + V);

		finalColor.rgb += calcBlinnPhongLighting(vOut.normalW, -light.dirW, H);
	}

	return finalColor;
}