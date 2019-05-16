__import Raytracing;
__import GGXGICommon;
#include "HostDeviceSharedMacros.h"
#include "ShaderTypes.h"

struct PrimaryRayData
{
    float4 color;
    uint depth;
    float hitT;
	float distance;
};

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

float4 debugVertex(float3 posW, float3 normalW)
{
    float3 V = normalize(gCamera.posW - posW);

    float4 color = float4(0, 0, 0, 1);

    for (uint l = 0; l < gLightsCount; l++)
    {
        LightData light = gLights[l];
        float3 H = normalize(-light.dirW + V);

        color.rgb += calcBlinnPhongLighting(normalize(normalW), normalize(-light.dirW), H);
    }

    return color;
}

float3 getReflectionColor(float3 worldOrigin, VertexOut v, float3 worldRayDir, uint hitDepth, float distance)
{
    float3 reflectColor = float3(0, 0, 0);
    if (hitDepth < gMaxDepth)
    {
		//fog
        float f = (gDoFog) ? 1.f / exp(1.e-4f * distance * distance) : 1.f;
			  
        PrimaryRayData secondaryRay;
        //secondaryRay.color = float4(1.f, 1.f, 1.f, 1.f);
        secondaryRay.depth.r = hitDepth + 1;
		secondaryRay.distance = distance; 
        RayDesc ray;
        ray.Origin = worldOrigin;

        if (gMaterial.id == TORUS || gMaterial.id == DODECAHEDRON) 
		{
			ray.Direction = worldRayDir;
		}
		else
		{
			ray.Direction = reflect(worldRayDir, v.normalW);
		}
        
		ray.TMin = 0.001;
        ray.TMax = 100000;
        TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 2 /* ray index*/, hitProgramCount, 2, ray, secondaryRay);
        reflectColor +=  f*secondaryRay.color.rgb + (1.-f);// secondaryRay.color.rgb + color;

        if (gMaterial.id != PERFECT_MIRROR && gMaterial.id != TORUS && gMaterial.id != DODECAHEDRON)
        {
            float falloff = max(1, (secondaryRay.hitT * secondaryRay.hitT));
            reflectColor *= 20 / falloff;
        }
    }
    return reflectColor;
}

float4 tracePrimaryRay(RayDesc ray)
{
    PrimaryRayData hitData;
    hitData.depth = 0;
	hitData.distance= 0;
    TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 2 /* ray index*/, hitProgramCount, 2, ray, hitData);
    return hitData.color;
}

float4 tracePrimaryRay(float3 posW)
{
    RayDesc ray;
    ray.Origin = gCamera.posW;
	//ray.Direction = rayDirs[DispatchRaysIndex().xy].xyz;
    ray.Direction = normalize(posW.xyz - gCamera.posW);

    ray.TMin = 0;
    ray.TMax = 100000;
    return tracePrimaryRay(ray);
}

/**  Evaluates direct lighting without shadows as a raster program would do.
*/
float3 raster(ShadingData sd)
{
    float3 finalColor = float3(0, 0, 0);

    [unroll]
    for (uint l = 0; l < gLightsCount; l++)
    {
        finalColor += evalMaterial(sd, gLights[l], 1).color.rgb;
    }

    return finalColor;
}

/**  Evaluates direct lighting using rays for shadows only.
*/
float3 directWithShadows(ShadingData sd)
{
    float3 finalColor = float3(0, 0, 0);

    [unroll]
    for (uint i = 0; i < gLightsCount; i++)
    {
        float3 dirToLight = gLights[i].posW - sd.posW;
        float distToLight = length(dirToLight);

        float3 L;
        if (gLights[i].type == LightPoint)
        {
            L = normalize(dirToLight);
        }
        else
        {
            L = -normalize(gLights[i].dirW);
        }
        
        float visFactor = shootShadowRay(sd.posW, L, 0.001, distToLight);

        finalColor += visFactor * evalMaterial(sd, gLights[i], 1).color.xyz;
    }

    return finalColor;
}