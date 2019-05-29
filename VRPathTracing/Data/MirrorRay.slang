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
__import Raytracing;
__import MirrorCommon;
__import GGXGICommon;
__import Helpers;
__import Hyperbolic;
#include "ShaderTypes.h"

[shader("anyhit")]
void primaryAnyHit(inout PrimaryRayData rayData, BuiltInTriangleIntersectionAttributes attribs)
{
    if (evalRtAlphaTest(attribs))
        IgnoreHit();
}

[shader("miss")]
void primaryMiss(inout PrimaryRayData hitData)
{
    // Sample the environment map
    float2 uv = dirToSphericalCrd(WorldRayDirection());
    float2 dims;
    gEnvMap.GetDimensions(dims.x, dims.y);
    hitData.color = gEnvMap[uint2(uv * dims)];
    hitData.hitT = -1;
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

    // translate the ray's origin
	if ( gMaterial.id == TORUS)
	{
	    uint3 indices = getIndices(triangleIndex);
       
        float3 v1 = asfloat(gPositions.Load3((indices[0] * 3) * 4));
			   v1 = mul(float4(v1, 1.f), gWorldMat[0]).xyz;

		float3 v2 = asfloat(gPositions.Load3((indices[1] * 3) * 4));
		       v2 = mul(float4(v2, 1.f), gWorldMat[0]).xyz;

		float3 v3 = asfloat(gPositions.Load3((indices[2] * 3) * 4));
	           v3 = mul(float4(v3, 1.f), gWorldMat[0]).xyz;

		float len1 = length(v2 - v1);
		float len2 = length(v3 - v1);

		//VertexOut v1 = getVertexAttributes(triangleIndex, float3(0,0,1));
		//VertexOut v2 = getVertexAttributes(triangleIndex, float3(0,1,0));
		//VertexOut v3 = getVertexAttributes(triangleIndex, float3(1,0,0));

		//float len1 = length(v2.posW - v1.posW);
		//float len2 = length(v3.posW - v1.posW);

		if (len1 < len2)
		{
			posW = posW + len1*v.normalW;
		}
		else
		{
			posW = posW + len2*v.normalW;
		}

    }
    else if (gMaterial.id == SEIFERT_WEBER_DODECAHEDRON/*SEIFERT_WEBER_DODECAHEDRON || gMaterial.id == MIRRORED_DODECAHEDRON*/)
    {
        // Debug
        bool foundFace = false;
        //
        
        for (int i = 0; i < 12; ++i)
        {
            float epsilon = 0.2f;
            v.normalW = normalize(v.normalW);
            float3 faceNormal = normalize(gFaces[i].normal);
            float dotValue = v.normalW.x * faceNormal.x + v.normalW.y * faceNormal.y + v.normalW.z * faceNormal.z;
            if (abs(1.f + dotValue) < epsilon)
            {
                Ray newRay;
                //if (gMaterial.id == SEIFERT_WEBER_DODECAHEDRON)
                //{
                    //newRay = func_new_ray_direction(gFaces[i].invTransform, posW, rayDirW);            
                /*}
                else
                {*/
                    newRay = perfectReflection(gFaces[i].transform, posW, rayDirW);
                //}
               
                posW = newRay.origin;
                rayDirW = normalize(newRay.dir);
                break;
            }
        }
    }
    
    hitData.distance += hitT;

	float3 reflectColor = getReflectionColor(posW, v, rayDirW, hitData.depth.r, hitData.distance);


    if (gMaterial.id == PERFECT_MIRROR || gMaterial.id == TORUS || gMaterial.id == SEIFERT_WEBER_DODECAHEDRON/*SEIFERT_WEBER_DODECAHEDRON || gMaterial.id == MIRRORED_DODECAHEDRON*/)
    {
        hitData.color.rgb = reflectColor;
    }
    else
    {
        ShadingData sd = prepareShadingData(v, gMaterial, rayOrigW, 0);
        float3 color = directWithShadows(sd);
        hitData.color.rgb = color;

        // A very non-PBR inaccurate way to do reflections
        float roughness = min(0.5, max(1e-8, sd.roughness));
        hitData.color.rgb += sd.specular * reflectColor * (roughness * roughness);
        hitData.color.rgb += sd.emissive;
    }

    hitData.hitT = hitT;
    hitData.color.a = 1;
}