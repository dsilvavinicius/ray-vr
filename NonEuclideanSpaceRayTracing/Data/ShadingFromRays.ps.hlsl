__import ShaderCommon;
//__import Shading;
__import DefaultVS;

float3 calcBlinnPhongLighting(float3 N, float3 L, float3 H)
{
	float3 ambient = float3(0.329412f, 0.223529f, 0.027451f);
	float3 diffuse = float3(0.780392f, 0.568627f, 0.113725f);
	float3 specular = float3(0.992157f, 0.941176f, 0.807843f);
	float shininess = 27.8974f;

	float3 Id = diffuse * saturate(dot(N, L));
	float3 Is = specular * pow(saturate(dot(N, H)), shininess);

	return ambient + Id + Is;
}

float4 main(VertexOut vOut) : SV_TARGET
{
	float3 V = normalize(vOut.prevPosH.xyz - vOut.posW);

	float4 finalColor = float4(0, 0, 0, 1);

	for (uint l = 0; l < gLightsCount; l++)
	{
		LightData light = gLights[l];
		float3 H = normalize(-light.dirW + V);

		finalColor.rgb += calcBlinnPhongLighting(normalize(vOut.normalW), normalize(-light.dirW), H);
	}

	return finalColor;
}