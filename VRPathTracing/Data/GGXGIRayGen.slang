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
import ShaderCommon;
import Shading;
import Helpers;
import GGXGICommon;
import MirrorCommon;
#include "ShaderTypes.h"

[shader("raygeneration")]
void GGXGlobalIllumRayGen()
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    uint2 launchDim = DispatchRaysDimensions().xy;

    // Initialize our random number generator
    uint randSeed = rand_init(launchIndex.x + launchIndex.y * launchDim.x, gFrameCount, 16);

    // Load g-buffer data
    float4 posW = gPos[launchIndex];
    float3 normW = gNorm[launchIndex].xyz;
    uint triangleIndex = (uint) gNorm[launchIndex].w;
    float4 diffuse = gDiffuseMatl[launchIndex];
    float4 specRough = gSpecMatl[launchIndex];
    float4 emissive = gEmissive[launchIndex];
    float3 geomN = gExtraMatl[launchIndex].xyz;
    uint matId = (uint) gExtraMatl[launchIndex].w;

    // Does this g-buffer pixel contain a valid piece of geometry?  (0 in pos.w for invalid)
    bool isGeometryValid = (posW.w != 0.0f);

    // Extract and compute some material and geometric parameters
    float roughness = specRough.a * specRough.a;
    float3 V = normalize(gCamera.posW - posW.xyz);

    // Make sure our normal is pointed the right direction
    if (dot(normW, V) <= 0.0f)
        normW = -normW;

    // Make sure out geometric normal is pointed in the right direction
    if (dot(geomN, V) <= 0.0f)
        geomN = -geomN;

    float3 shadeColor = float3(0, 0, 0);

    // Do shading, if we have geometry here (otherwise, output the background color)
    if (isGeometryValid)
    {
        // Add any emissive color from primary rays
        shadeColor = gEmitMult * emissive.rgb;

        // Fill out ShadingData struct with GBuffer data
        ShadingData sd = initShadingData();
        sd.posW = posW.xyz;
        sd.N = normW;
        sd.V = V;
        sd.diffuse = diffuse.rgb;
        sd.specular = specRough.rgb;
        sd.linearRoughness = specRough.a;
        sd.roughness = roughness;

        switch (matId)
        {
            case RASTER:
            {
                shadeColor += raster(sd);
                break;
            }
            case RASTER_PLUS_SHADOWS:
            {
                shadeColor += directWithShadows(sd);
                break;
            }

            case TORUS:
            case REFLECTIONS:
            case PERFECT_MIRROR:
            case SEIFERT_WEBER_DODECAHEDRON:
            case MIRRORED_DODECAHEDRON:
            {
                if (launchIndex.x % gRayStride == 0 && launchIndex.y % gRayStride == 0)
                {
                    float3 rayDirWNotNorm = posW.xyz - gCamera.posW;
                    float hitT = length(rayDirWNotNorm);
                    float3 rayDirW = normalize(rayDirWNotNorm);

                    PrimaryRayData hitData;
                    hitData.color = float4(0.f, 0.f, 0.f, 0.f);
                    hitData.depth = 0.f;
                    hitData.hitT = 0.f;
                    hitData.distance = 0.f;

                    evaluateReflectionRay(hitData, sd, sd.posW, gCamera.posW, rayDirW, hitT, triangleIndex, matId);
                    shadeColor += hitData.color.rgb;
                }
                break;
            }

            case PATH_TRACING: // Path tracing
            {
                // Direct lighting to a random light in the scene
                if (gDoDirectGI)
                    shadeColor += ggxDirect(randSeed, sd);

                // Indirect lighting for global illumination
                if (gDoIndirectGI && (gMaxDepth > 0))
                    shadeColor += ggxIndirect(randSeed, sd, geomN);

                // Zero out any bad samples
                bool colorsNan = any(isnan(shadeColor));
                shadeColor = colorsNan ? float3(0, 0, 0) : shadeColor;
                break;
            }
        }
    }
    
    gOutput[launchIndex] = float4(shadeColor, 1.0f);
}
