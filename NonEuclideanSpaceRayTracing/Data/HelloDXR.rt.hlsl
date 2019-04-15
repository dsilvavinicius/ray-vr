/***************************************************************************
# Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************/

//#define DEBUG_VERTEX_ATTRIBS
//#define DEBUG_RAY_GENERATION

RWTexture2D<float4> gOutput;
RWTexture2D<float4> gRightOutput;

RWTexture2D<float4> gLeftEyePos;
RWTexture2D<float4> gRightEyePos;
__import Raytracing;

shared cbuffer PerFrameCB
{
    float4x4 invView;
    float4x4 invRightView;
    float2 viewportDims;
    float tanHalfFovY;

    float4x4 invViewProj;
	float4x4 invRightViewProj;

	float3 RightCamPos;
	float3 RightCamU;
	float3 RightCamV;
	float3 RightCamW;

	float4 clearColor;
    bool leftEyeOnly;
};

struct PrimaryRayData
{
    float4 color;
    uint depth;
    float hitT;
};

struct ShadowRayData
{
    bool hit;
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

[shader("miss")]
void shadowMiss(inout ShadowRayData hitData)
{
    hitData.hit = false;
}

[shader("anyhit")]
void shadowAnyHit(inout ShadowRayData hitData, in BuiltInTriangleIntersectionAttributes attribs)
{
    hitData.hit = true;
}

[shader("miss")]
void primaryMiss(inout PrimaryRayData hitData)
{
    hitData.color = float4(0.38f, 0.52f, 0.10f, 1);
    hitData.hitT = -1;
}

bool checkLightHit(uint lightIndex, float3 origin)
{
    float3 direction = gLights[lightIndex].posW - origin;
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = normalize(direction);
    ray.TMin = 0.001;
    ray.TMax = max(0.01, length(direction));

    ShadowRayData rayData;
    rayData.hit = true;
    TraceRay(gRtScene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 1 /* ray index */, hitProgramCount, 1, ray, rayData);
    return rayData.hit;
}

float3 getReflectionColor(float3 worldOrigin, VertexOut v, float3 worldRayDir, uint hitDepth)
{
    float3 reflectColor = float3(0, 0, 0);
    if (hitDepth == 0)
    {
        PrimaryRayData secondaryRay;
        secondaryRay.depth.r = 1;
        RayDesc ray;
        ray.Origin = worldOrigin;
        ray.Direction = reflect(worldRayDir, v.normalW);
        ray.TMin = 0.001;
        ray.TMax = 100000;
        TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, hitProgramCount, 0, ray, secondaryRay);
        reflectColor = secondaryRay.hitT == -1 ? 0 : secondaryRay.color.rgb;
        float falloff = max(1, (secondaryRay.hitT * secondaryRay.hitT));
        reflectColor *= 20 / falloff;
    }
    return reflectColor;
}

[shader("closesthit")]
void primaryClosestHit(inout PrimaryRayData hitData, in BuiltInTriangleIntersectionAttributes attribs)
{
    // Get the hit-point data
    float3 rayOrigW = WorldRayOrigin();
    float3 rayDirW = WorldRayDirection(); 
    float hitT = RayTCurrent();
    uint triangleIndex = PrimitiveIndex();

    float3 posW = rayOrigW + hitT * rayDirW;
    // prepare the shading data
    VertexOut v = getVertexAttributes(triangleIndex, attribs);
    
#ifdef DEBUG_VERTEX_ATTRIBS
    //hitData.color = debugVertex(v.posW, v.normalW);
    float len = length(gLeftRayDirs[DispatchRaysIndex().xy].xyz - posW);
    hitData.color = float4(len, len, len, 1.f);
    //hitData.color = float4(posW, 1.f);
    //hitData.hitT = hitT;
#else
    ShadingData sd = prepareShadingData(v, gMaterial, rayOrigW, 0);

    // Shoot a reflection ray
    float3 reflectColor = getReflectionColor(posW, v, rayDirW, hitData.depth.r);
    float3 color = 0;

    [unroll]
    for (int i = 0; i < gLightsCount; i++)
    {        
        if (checkLightHit(i, posW) == false)
        {
            color += evalMaterial(sd, gLights[i], 1).color.xyz;
        }
    }

    hitData.color.rgb = color;
    hitData.hitT = hitT;
    // A very non-PBR inaccurate way to do reflections
    float roughness = min(0.5, max(1e-8, sd.roughness));
    hitData.color.rgb += sd.specular * reflectColor * (roughness * roughness);
    hitData.color.rgb += sd.emissive;
    hitData.color.a = 1;
#endif
}

float4 tracePrimaryRay(RayDesc ray)
{
	PrimaryRayData hitData;
	hitData.depth = 0;
	TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, hitProgramCount, 0, ray, hitData);
	return hitData.color;
}

float4 tracePrimaryRay(float4x4 invView, float2 ndc)
{
    float4 nearNdc = float4(ndc, -1.f, 1.f);
    float4 farNdc = float4(ndc, 1, 1);

    float4 nearH = mul(nearNdc, invView);
    float4 farH = mul(farNdc, invView);
    float3 nearW = nearH.xyz / nearH.w;
    float3 farW = farH.xyz / farH.w;

	RayDesc ray;
    ray.Origin = nearW;
	ray.Direction = normalize(farW - nearW);

	ray.TMin = 0;
	ray.TMax = 100000;

    return tracePrimaryRay(ray);
}

float4 tracePrimaryRay(float4x4 invView, float2 d, float aspect)
{
    RayDesc ray;
    ray.Origin = invView[3].xyz;

    // We negate the Z exis because the 'view' matrix is generated using a 
    // Right Handed coordinate system with Z pointing towards the viewer
    // The negation of Z axis is needed to get the rays go out in the
    // direction away from the viewer.
    // The negation of Y axis is needed because the texel coordinate system,
    // used in the UAV we write into using launchIndex
    // has the Y axis flipped with respect to the camera Y axis
    // (0 is at the top and 1 at the bottom)
    ray.Direction = normalize((d.x * invView[0].xyz * tanHalfFovY * aspect)
                        - (d.y * invView[1].xyz * tanHalfFovY) - invView[2].xyz);
    
    ray.TMin = 0;
    ray.TMax = 100000;

    return tracePrimaryRay(ray);
}

float4 tracePrimaryRay(float2 ndc, float3 posW, float3 camU, float3 camV, float3 camW)
{
	RayDesc ray;
	ray.Origin = posW; // Start our ray at the world-space camera position
	float3 rayDir = ndc.x * camU + ndc.y * camV + camW;
	ray.Direction = normalize(rayDir);

	ray.TMin = 0;
	ray.TMax = 100000;

    return tracePrimaryRay(ray);
}

float4 tracePrimaryRay(float3 origin, RWTexture2D<float4> rayDirs)
{
	float3 posW = rayDirs[DispatchRaysIndex().xy].xyz;

	float epsilon = 1.0e-5;
	float3 diff = posW - clearColor.xyz;
	if (dot(diff, diff) < epsilon)
	{
		return clearColor;
	}
	else
	{
#ifndef DEBUG_RAY_GENERATION
		RayDesc ray;
		ray.Origin = origin;
		//ray.Direction = rayDirs[DispatchRaysIndex().xy].xyz;
		ray.Direction = normalize(posW.xyz - origin);

		ray.TMin = 0;
		ray.TMax = 100000;
        return tracePrimaryRay(ray);
#else
        return float4(posW, 1.f);
#endif
    }
}

// In this version the ray directions are created using the inverse view matrix.
void traceRaysInvViewProj()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 pixelCenter = (launchIndex + float2(0.5f, 0.5f)) / DispatchRaysDimensions().xy;
    float2 ndc = float2(2, -2) * pixelCenter + float2(-1, 1);

    gOutput[launchIndex] = tracePrimaryRay(invViewProj, ndc);

    if (!leftEyeOnly)
    {
        gRightOutput[launchIndex] = tracePrimaryRay(invRightViewProj, ndc);
    }
}

void traceRaysInvView()
{
    uint3 launchIndex = DispatchRaysIndex();
    float2 d = (((launchIndex.xy + 0.5) / viewportDims) * 2.f - 1.f);
    float aspectRatio = viewportDims.x / viewportDims.y;

    gOutput[launchIndex.xy] = tracePrimaryRay(invView, d, aspectRatio);

    if (!leftEyeOnly)
    {
        gRightOutput[launchIndex.xy] = tracePrimaryRay(invRightView, d, aspectRatio);
    }
}

// In this version the ray directions are created using the camera vectors. 
void traceRaysCamVecs()
{
	uint3 launchIndex = DispatchRaysIndex();
	float2 pixelCenter = (launchIndex.xy + float2(0.5f, 0.5f)) / DispatchRaysDimensions().xy;
	float2 ndc = float2(2, -2) * pixelCenter + float2(-1, 1);

    gOutput[launchIndex.xy] = tracePrimaryRay(ndc, gCamera.posW, gCamera.cameraU, gCamera.cameraV, gCamera.cameraW);
    
    if (!leftEyeOnly)
    {
        gRightOutput[launchIndex.xy] = tracePrimaryRay(ndc, RightCamPos, RightCamU, RightCamV, RightCamW);
    }
}

// In this version the ray directions are provided in a texture.
void traceRaysTex()
{
	gOutput[DispatchRaysIndex().xy] = tracePrimaryRay(gCamera.posW, gLeftEyePos);

    if (!leftEyeOnly)
    {
        gRightOutput[DispatchRaysIndex().xy] = tracePrimaryRay(RightCamPos, gRightEyePos);
    }
}

[shader("raygeneration")]
void rayGen()
{
#if VERSION == 0
    traceRaysInvViewProj();
#elif VERSION == 1
    traceRaysInvView();
#elif VERSION == 2
    traceRaysCamVecs();
#else
	traceRaysTex();
#endif
}
