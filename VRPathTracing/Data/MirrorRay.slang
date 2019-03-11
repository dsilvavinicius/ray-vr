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

__import Raytracing;
__import MirrorCommon;
__import GGXGICommon;
__import Helpers;

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