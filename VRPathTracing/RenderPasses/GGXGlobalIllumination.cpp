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
#include "GGXGlobalIllumination.h"

// Some global vars, used to simplify changing shader locations
namespace {
    // Shader files
    const char* kFileRayGen = "GGXGIRayGen.slang";
    
	const std::array<const char*, 3> kRayNames =
	{
		"GGXGIShadowRay",
		"GGXGIIndirectRay",
		"ReflectionRay"
	};

	// Entry-point names
    const char* kEntryPointRayGen = "GGXGlobalIllumRayGen";
};

GGXGlobalIllumination::SharedPtr GGXGlobalIllumination::create(const Dictionary &params)
{
    GGXGlobalIllumination::SharedPtr pPass(new GGXGlobalIllumination());

    // Load parameters from Python
    if (params.keyExists("useEmissives"))    pPass->mUseEmissiveGeom = params["useEmissives"];
    if (params.keyExists("doDirectLight"))   pPass->mDoDirectGI = params["doDirectLight"];
    if (params.keyExists("doIndirectLight")) pPass->mDoIndirectGI = params["doIndirectLight"];
	if (params.keyExists("doFog"))			 pPass->mDoFog = params["doFog"];
    if (params.keyExists("rayDepth"))        pPass->mUserSpecifiedRayDepth = params["rayDepth"];
    if (params.keyExists("randomSeed"))      pPass->mFrameCount = params["randomSeed"];
	if (params.keyExists("rayStride"))		 pPass->mRayStride = params["rayStride"];
	if (params.keyExists("torusDomainSize")) pPass->mTorusDomainSizeW = params["torusDomainSize"];
    if (params.keyExists("useBlackEnvMap"))  pPass->mEnvMapMode = params["useBlackEnvMap"] ? EnvMapMode::Black : EnvMapMode::Scene;

    return pPass;
}

Dictionary GGXGlobalIllumination::getScriptingDictionary() const
{
    Dictionary serialize;
    serialize["useEmissives"] = mUseEmissiveGeom;
    serialize["doDirectLight"] = mDoDirectGI;
    serialize["doIndirectLight"] = mDoIndirectGI;
	serialize["doFog"] = mDoFog;
    serialize["rayDepth"] = mUserSpecifiedRayDepth;
    serialize["randomSeed"] = mFrameCount;
	serialize["randomSeed"] = mRayStride;
	serialize["torusDomainSize"] = mTorusDomainSizeW;
    serialize["useBlackEnvMap"] = mEnvMapMode == EnvMapMode::Black;
    return serialize;
}

RenderPassReflection GGXGlobalIllumination::reflect(void) const
{
    RenderPassReflection r;
    r.addInput("posW", "");
    r.addInput("normW", "");
    r.addInput("diffuseOpacity", "");
    r.addInput("specRough", "");
    r.addInput("emissive", "");
    r.addInput("matlExtra", "");

    r.addOutput("output", "").format(ResourceFormat::RGBA32Float).bindFlags(Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess | Resource::BindFlags::RenderTarget);
    return r;
}

void GGXGlobalIllumination::initialize(RenderContext* pContext, const RenderData* pRenderData)
{
    mpBlackHDR = Texture::create2D(128, 128, ResourceFormat::RGBA32Float, 1u, 1u, nullptr, Resource::BindFlags::ShaderResource | Resource::BindFlags::RenderTarget);
    pContext->clearRtv(mpBlackHDR->getRTV().get(), vec4(0.0f, 0.0f, 0.0f, 1.0f));

    mpState = RtState::create();
    mpState->setMaxTraceRecursionDepth(mMaxPossibleRayDepth);

    RtProgram::Desc desc;
    desc.addShaderLibrary(kFileRayGen);
    desc.setRayGen(kEntryPointRayGen);

	for (int i = 0; i < kRayNames.size(); ++i)
	{
		std::string shaderFile = kRayNames[i];
		shaderFile.append(".slang");
		desc.addShaderLibrary(shaderFile);
		
		std::string entryMiss = kRayNames[i];
		entryMiss += "Miss";
		desc.addMiss(i, entryMiss);

		std::string entryClosestHit = kRayNames[i];
		entryClosestHit += "ClosestHit";

		std::string entryAnyHit = kRayNames[i];
		entryAnyHit += "AnyHit";
		desc.addHitGroup(i, entryClosestHit, entryAnyHit);
	}

    // Now that we've passed all our shaders in, compile and (if available) setup the scene
    mpProgram = RtProgram::create(desc);
    mpState->setProgram(mpProgram);

    if (mpProgram != nullptr && mpScene != nullptr)
    {
        mpSceneRenderer = RtSceneRenderer::create(mpScene);
        mpVars = RtProgramVars::create(mpProgram, mpScene);
    }

    mIsInitialized = true;
}

void GGXGlobalIllumination::execute(RenderContext* pContext, const RenderData* pData)
{
    // On first execution, run some initialization
    if (!mIsInitialized)
    {
        initialize(pContext, pData);
    }

    // Get our output buffer and clear it
    Texture::SharedPtr pDstTex = pData->getTexture("output");
    pContext->clearUAV(pDstTex->getUAV().get(), vec4(0.0f, 0.0f, 0.0f, 1.0f));

    if (pDstTex == nullptr || mpScene == nullptr) return;

    // Set our variables into the global HLSL namespace
    auto globalVars = mpVars->getGlobalVars();

    ConstantBuffer::SharedPtr pCB = globalVars->getConstantBuffer("GlobalCB");
    pCB["gMinT"] = 1.0e-3f;
    pCB["gFrameCount"] = mFrameCount++;
    pCB["gDoIndirectGI"] = mDoIndirectGI;
    pCB["gDoDirectGI"] = mDoDirectGI;
	pCB["gDoFog"] = mDoFog;
    pCB["gMaxDepth"] = uint32_t(mUserSpecifiedRayDepth);
    pCB["gEmitMult"] = float(mUseEmissiveGeom ? mEmissiveGeomMult : 0.0f);
	pCB["gRayStride"] = mRayStride;
	pCB["gTorusDomainSizeW"] = mTorusDomainSizeW;

	pCB["gSphericalScale"] = mSphericalScale;
	pCB["gThickness"] = mThickness;
	pCB["gScale"] = mScale;

	ConstantBuffer::SharedPtr pHyperbolicCB = globalVars->getConstantBuffer("HyperbolicCB");
	pHyperbolicCB["gDodecahedronScale"] = mDodecahedronScale;

    globalVars->setTexture("gPos", pData->getTexture("posW"));
    globalVars->setTexture("gNorm", pData->getTexture("normW"));
    globalVars->setTexture("gDiffuseMatl", pData->getTexture("diffuseOpacity"));
    globalVars->setTexture("gSpecMatl", pData->getTexture("specRough"));
    globalVars->setTexture("gExtraMatl", pData->getTexture("matlExtra"));
    globalVars->setTexture("gEmissive", pData->getTexture("emissive"));
    globalVars->setTexture("gOutput", pDstTex);

    const Texture::SharedPtr& pEnvMap = mpScene->getEnvironmentMap();
    globalVars->setTexture("gEnvMap", (mEnvMapMode == EnvMapMode::Black || pEnvMap == nullptr) ? mpBlackHDR : pEnvMap);

    // Launch our ray tracing
    mpSceneRenderer->renderScene(pContext, mpVars, mpState, mRayLaunchDims);
}

void GGXGlobalIllumination::renderUI(Gui* pGui, const char* uiGroup)
{
    bool changed = pGui->addIntVar("Max RayDepth", mUserSpecifiedRayDepth, 0, mMaxPossibleRayDepth);
    changed |= pGui->addCheckBox("Compute direct illumination", mDoDirectGI);
    changed |= pGui->addCheckBox("Compute global illumination", mDoIndirectGI);
	changed |= pGui->addCheckBox("Fog", mDoFog);
	changed |= pGui->addIntSlider("Ray Stride", mRayStride, 1, 8);
	changed |= pGui->addFloatSlider("Torus Domain Size", mTorusDomainSizeW, 0.f, 10.f);
	changed |= pGui->addFloatSlider("Mirrored Dodecahedron Scale", mDodecahedronScale, 0.1f, 1.f, false, "%5f");

	changed |= pGui->addFloatSlider("Dodecahedron Scale", mSphericalScale, 0.f, 1.f);
	changed |= pGui->addFloatSlider("Thickness", mThickness, 0.f, 0.1f);
	changed |= pGui->addFloatSlider("Scale", mScale, 0.f, 10.f);

    pGui->addSeparator();

    const static Gui::RadioButtonGroup kButtons =
    {
        {(uint32_t)EnvMapMode::Scene, "Scene", false},
        {(uint32_t)EnvMapMode::Black, "Black", true}
    };

    pGui->addText("Environment Map");
    changed |= pGui->addRadioButtons(kButtons, (uint32_t&)mEnvMapMode);

    if (changed) mPassChangedCB();
}

void GGXGlobalIllumination::setScene(const std::shared_ptr<Scene>& pScene)
{
    // Stash a copy of the scene
    mpScene = std::dynamic_pointer_cast<RtScene>(pScene);
    if (!mpScene) return;

    mpSceneRenderer = RtSceneRenderer::create(mpScene);
    if(mpProgram != nullptr) mpVars = RtProgramVars::create(mpProgram, mpScene);
}

void GGXGlobalIllumination::onResize(uint32_t width, uint32_t height)
{
    mRayLaunchDims = uvec3(width, height, 1);
}
