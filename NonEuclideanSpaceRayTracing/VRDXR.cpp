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
#include "VRDXR.h"

static const glm::vec4 kClearColor(0.38f, 0.52f, 0.10f, 1);
static const std::string kDefaultScene = "Arcade/Arcade.fscene";

std::string to_string(const vec3& v)
{
	std::string s;
	s += "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
	return s;
}

void VRDXR::onGuiRender(SampleCallbacks* pSample, Gui* pGui)
{
	pGui->addCheckBox("Ray Trace", mRayTrace);
	if (pGui->addButton("Load Scene"))
	{
		std::string filename;
		if (openFileDialog(Scene::kFileExtensionFilters, filename))
		{
			loadScene(filename, pSample->getCurrentFbo().get());
		}
	}

	for (uint32_t i = 0; i < mpScene->getLightCount(); i++)
	{
		std::string group = "Point Light" + std::to_string(i);
		mpScene->getLight(i)->renderUI(pGui, group.c_str());
	}

	// VR
	if (VRSystem::instance())
	{
		pGui->addCheckBox("Display VR FBO", mShowStereoViews);
	}

	if (pGui->addDropdown("Submission Mode", mSubmitModeList, (uint32_t&)mRenderMode))
	{
		setRenderMode();
	}
}

/*
void StereoRendering::onGuiRender(SampleCallbacks* pSample, Gui* pGui)
{
	if (pGui->addButton("Load Scene"))
	{
		loadScene();
	}
}
*/

void VRDXR::loadScene(const std::string& filename, const Fbo* pTargetFbo)
{
	mpScene = RtScene::loadFromFile(filename, RtBuildFlags::None, Model::LoadFlags::RemoveInstancing);
	Model::SharedPtr pModel = mpScene->getModel(0);
	float radius = pModel->getRadius();

	mpCamera = mpScene->getActiveCamera();
	assert(mpCamera);

	mCamController.attachCamera(mpCamera);

	Sampler::Desc samplerDesc;
	samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
	Sampler::SharedPtr pSampler = Sampler::create(samplerDesc);
	pModel->bindSamplerToMaterials(pSampler);

	// Update the controllers
	mpSceneRenderer = SceneRenderer::create(mpScene);
	mpRtVars = RtProgramVars::create(mpRaytraceProgram, mpScene);
	mpRtRenderer = RtSceneRenderer::create(mpScene);

	setRenderMode();
}

void VRDXR::onLoad(SampleCallbacks* pSample, RenderContext* pRenderContext)
{
	if (gpDevice->isFeatureSupported(Device::SupportedFeatures::Raytracing) == false)
	{
		logErrorAndExit("Device does not support raytracing!", true);
	}

	suppress_deprecation
		mSPSSupported = gpDevice->isExtensionSupported("VK_NVX_multiview_per_view_attributes");

	initVR(pSample->getCurrentFbo().get());

	mpGraphicsState = GraphicsState::create();
	setRenderMode();

	RtProgram::Desc rtProgDesc;
	rtProgDesc.addShaderLibrary("HelloDXR.rt.hlsl").setRayGen("rayGen");
	rtProgDesc.addHitGroup(0, "primaryClosestHit", "").addMiss(0, "primaryMiss");
	rtProgDesc.addHitGroup(1, "", "shadowAnyHit").addMiss(1, "shadowMiss");

	mpRaytraceProgram = RtProgram::create(rtProgDesc);

	loadScene(kDefaultScene, pSample->getCurrentFbo().get());

	GraphicsProgram::Desc progDesc;
	progDesc.addShaderLibrary("StereoRendering.vs.hlsl").vsEntry("main").addShaderLibrary("StereoRendering.ps.hlsl").psEntry("main").addShaderLibrary("StereoRendering.gs.hlsl").gsEntry("main");
	mpStereoProgram = GraphicsProgram::create(progDesc);
	mpStereoVars = GraphicsVars::create(mpStereoProgram->getReflector());

	GraphicsProgram::Desc rayTexDesc;
	rayTexDesc.addShaderLibrary("StereoRendering.vs.hlsl").vsEntry("main").addShaderLibrary("Proj2Rays.ps.hlsl").psEntry("main").addShaderLibrary("StereoRendering.gs.hlsl").gsEntry("main");
	mpRayTexProgram = GraphicsProgram::create(rayTexDesc);
	mpRayTexVars = GraphicsVars::create(mpRayTexProgram->getReflector());

	mpRtState = RtState::create();
	mpRtState->setProgram(mpRaytraceProgram);
	mpRtState->setMaxTraceRecursionDepth(3); // 1 for calling TraceRay from RayGen, 1 for calling it from the primary-ray ClosestHitShader for reflections, 1 for reflection ray tracing a shadow ray
}

void VRDXR::renderRaster(RenderContext* pContext)
{
	mpGraphicsState->setProgram(mpStereoProgram);
	pContext->setGraphicsVars(mpStereoVars);

	pContext->pushGraphicsState(mpGraphicsState);

	// Render
	mpSceneRenderer->renderScene(pContext);

	// Restore the state
	pContext->popGraphicsState();
}

void VRDXR::setPerFrameVars(const Fbo* pTargetFbo, const CameraData& rightEyeCamData)
{
	PROFILE("setPerFrameVars");
	GraphicsVars* pVars = mpRtVars->getGlobalVars().get();
	ConstantBuffer::SharedPtr pCB = pVars->getConstantBuffer("PerFrameCB");

	VRDisplay* hmd = VRSystem::instance()->getHMD().get();

	pCB["invView"] = glm::inverse(mpCamera->getViewMatrix());
	pCB["invRightView"] = glm::inverse(mpCamera->getRightEyeViewMatrix());
	
	pCB["RightCamPos"] = rightEyeCamData.posW;
	pCB["RightCamU"] = rightEyeCamData.cameraU;
	pCB["RightCamV"] = rightEyeCamData.cameraV;
	pCB["RightCamW"] = rightEyeCamData.cameraW;

	pCB["viewportDims"] = vec2(pTargetFbo->getWidth(), pTargetFbo->getHeight());
	float fovY = hmd->getFovY();
	pCB["tanHalfFovY"] = tanf(fovY * 0.5f);

	pVars->setTexture("gLeftRayDirs", mpRayDirs[0]);
	pVars->setTexture("gRightRayDirs", mpRayDirs[1]);
}

void VRDXR::renderRT(RenderContext* pContext, const Fbo* pTargetFbo)
{
	PROFILE("renderRT");
	
	CameraData rightEyeCamData = calculateRightEyeParams();
	calcRayDirs(pContext, rightEyeCamData);

	/*pContext->blit(pTargetFbo->getRenderTargetView(0)->getResource()->getSRV(0, 0, 1), mpRayDirs[0]->getRTV());
	pContext->blit(pTargetFbo->getRenderTargetView(0)->getResource()->getSRV(0, 1, 1), mpRayDirs[1]->getRTV());

	mpGraphicsState->setFbo(mpVrFbo->getFbo());
	pContext->clearFbo(mpVrFbo->getFbo().get(), kClearColor, 1.0f, 0, FboAttachmentType::All);

	setPerFrameVars(pTargetFbo, rightEyeCamData);

	pContext->clearUAV(mpRtOut[ 0 ]->getUAV().get(), kClearColor);
	pContext->clearUAV(mpRtOut[ 1 ]->getUAV().get(), kClearColor);
	
	mpRtVars->getRayGenVars()->setTexture("gOutput", mpRtOut[ 0 ]);
	mpRtVars->getRayGenVars()->setTexture("gRightOutput", mpRtOut[ 1 ]);
	
	mpRtRenderer->renderScene(pContext, mpRtVars, mpRtState, uvec3(pTargetFbo->getWidth(), pTargetFbo->getHeight(), 1), mpCamera.get());
	pContext->blit(mpRtOut[ 0 ]->getSRV(), pTargetFbo->getRenderTargetView( 0 )->getResource()->getRTV(0, 0, 1));
	pContext->blit(mpRtOut[ 1 ]->getSRV(), pTargetFbo->getRenderTargetView( 0 )->getResource()->getRTV(0, 1, 1));*/
}

void VRDXR::calcRayDirs(RenderContext* pContext, const CameraData& rightEyeCamData)
{
	ConstantBuffer::SharedPtr pCB = mpRayTexVars->getConstantBuffer("PerFrameCB");
	pCB["gRightEyePosW"] = rightEyeCamData.posW;

	mpGraphicsState->setProgram(mpRayTexProgram);
	pContext->setGraphicsVars(mpRayTexVars);

	pContext->pushGraphicsState(mpGraphicsState);

	// Render
	mpSceneRenderer->renderScene(pContext);

	// Restore the state
	pContext->popGraphicsState();
}

void VRDXR::onFrameRender(SampleCallbacks* pSample, RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo)
{
	PROFILE("submitStereo");
	VRSystem::instance()->refresh();
	
	pRenderContext->clearFbo(pTargetFbo.get(), kClearColor, 1.0f, 0, FboAttachmentType::All);

	if (mpScene)
	{
		pRenderContext->clearFbo(mpVrFbo->getFbo().get(), kClearColor, 1.0f, 0, FboAttachmentType::All);

		mpGraphicsState->setFbo(mpVrFbo->getFbo());
		//mpGraphicsState->setFbo(pTargetFbo);
		mCamController.update();

		if (mRayTrace)
		{
			renderRT(pRenderContext, mpVrFbo->getFbo().get());
			//renderRT(pRenderContext, pTargetFbo.get());
		}
		else
		{
			renderRaster(pRenderContext);
		}

		/*
		VRSystem* pVrSystem = VRSystem::instance();
		pVrSystem->submit(VRDisplay::Eye::Left, pTargetFbo->getColorTexture( 0 ), pRenderContext);
		pVrSystem->submit(VRDisplay::Eye::Right, pTargetFbo->getColorTexture( 0 ), pRenderContext);
		*/

		// Submit the views and display them
		mpVrFbo->submitToHmd(pRenderContext);
		blitTexture(pRenderContext, pTargetFbo.get(), mpVrFbo->getEyeResourceView(VRDisplay::Eye::Left), 0);
		blitTexture(pRenderContext, pTargetFbo.get(), mpVrFbo->getEyeResourceView(VRDisplay::Eye::Right), pTargetFbo->getWidth() / 2);
		
	}
}

/*
void StereoRendering::onFrameRender(SampleCallbacks* pSample, RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo)
{
	static uint32_t frameCount = 0u;

	pRenderContext->clearFbo(pTargetFbo.get(), kClearColor, 1.0f, 0, FboAttachmentType::All);

	if (mpSceneRenderer)
	{
		mpSceneRenderer->update(pSample->getCurrentTime());

		switch (mRenderMode)
		{
		case RenderMode::Mono:
			submitToScreen(pRenderContext, pTargetFbo);
			break;
		case RenderMode::SinglePassStereo:
			submitStereo(pRenderContext, pTargetFbo, true);
			break;
		case RenderMode::Stereo:
			submitStereo(pRenderContext, pTargetFbo, false);
			break;
		default:
			should_not_get_here();
		}
	}

	std::string message = pSample->getFpsMsg();
	message += "\nFrame counter: " + std::to_string(frameCount);

	pSample->renderText(message, glm::vec2(10, 10));

	frameCount++;
}
*/

bool VRDXR::onKeyEvent(SampleCallbacks* pSample, const KeyboardEvent& keyEvent)
{
	if (mCamController.onKeyEvent(keyEvent))
	{
		return true;
	}

	if (keyEvent.key == KeyboardEvent::Key::Space && keyEvent.type == KeyboardEvent::Type::KeyPressed)
	{
		mRayTrace = !mRayTrace;
		return true;
	}
	return false;
}

/*
bool StereoRendering::onKeyEvent(SampleCallbacks* pSample, const KeyboardEvent& keyEvent)
{
	if (keyEvent.key == KeyboardEvent::Key::Space && keyEvent.type == KeyboardEvent::Type::KeyPressed)
	{
		if (VRSystem::instance())
		{
			// Cycle through modes
			uint32_t nextMode = (uint32_t)mRenderMode + 1;
			mRenderMode = (RenderMode)(nextMode % (mSPSSupported ? 3 : 2));
			setRenderMode();
			return true;
		}
	}
	return mpSceneRenderer ? mpSceneRenderer->onKeyEvent(keyEvent) : false;
}
*/

bool VRDXR::onMouseEvent(SampleCallbacks* pSample, const MouseEvent& mouseEvent)
{
	return mCamController.onMouseEvent(mouseEvent);
}

/*
bool StereoRendering::onMouseEvent(SampleCallbacks* pSample, const MouseEvent& mouseEvent)
{
	return mpSceneRenderer ? mpSceneRenderer->onMouseEvent(mouseEvent) : false;
}
*/

void VRDXR::onResizeSwapChain(SampleCallbacks* pSample, uint32_t width, uint32_t height)
{
	// VR
	initVR(pSample->getCurrentFbo().get());
}

bool displaySpsWarning()
{
#ifdef FALCOR_D3D12
	logWarning("The sample will run faster if you use NVIDIA's Single Pass Stereo\nIf you have an NVIDIA GPU, download the NVAPI SDK and enable NVAPI support in FalcorConfig.h.\nCheck the readme for instructions");
#endif
	return false;
}

void VRDXR::initVR(Fbo* pTargetFbo)
{
	mSubmitModeList.clear();
	mSubmitModeList.push_back({ (int)RenderMode::Mono, "Render to Screen" });

	if (VRSystem::instance())
	{
		// Create the FBOs
		Fbo::Desc vrFboDesc;

		vrFboDesc.setColorTarget(0, pTargetFbo->getColorTexture(0)->getFormat());
		vrFboDesc.setDepthStencilTarget(pTargetFbo->getDepthStencilTexture()->getFormat());

		mpVrFbo = VrFbo::create(vrFboDesc);

		mSubmitModeList.push_back({ (int)RenderMode::Stereo, "Stereo" });

		if (mSPSSupported)
		{
			mSubmitModeList.push_back({ (int)RenderMode::SinglePassStereo, "Single Pass Stereo" });
		}
		else
		{
			displaySpsWarning();
		}

		VRDisplay* pDisplay = VRSystem::instance()->getHMD().get();
		ivec2 renderSize = pDisplay->getRecommendedRenderSize();

		mpRtOut[0] = Texture::create2D(renderSize.x, renderSize.y, ResourceFormat::RGBA16Float, 1, 1, nullptr, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::ShaderResource);
		mpRtOut[1] = Texture::create2D(renderSize.x, renderSize.y, ResourceFormat::RGBA16Float, 1, 1, nullptr, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::ShaderResource);
		
		mpRayDirs[0] = Texture::create2D(renderSize.x, renderSize.y, ResourceFormat::RGBA16Float, 1, 1, nullptr, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::ShaderResource | Resource::BindFlags::RenderTarget);
		mpRayDirs[1] = Texture::create2D(renderSize.x, renderSize.y, ResourceFormat::RGBA16Float, 1, 1, nullptr, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::ShaderResource | Resource::BindFlags::RenderTarget);

		//mpRaysFbo = Fbo::create();
		//mpRaysFbo->attachColorTarget();
	}
	else
	{
		msgBox("Can't initialize the VR system. Make sure that your HMD is connected properly");
	}
}

/*
void StereoRendering::submitStereo(RenderContext* pContext, Fbo::SharedPtr pTargetFbo, bool singlePassStereo)
{
	PROFILE("submitStereo");
	VRSystem::instance()->refresh();

	// Clear the FBO
	pContext->clearFbo(mpVrFbo->getFbo().get(), kClearColor, 1.0f, 0, FboAttachmentType::All);

	// update state
	if (singlePassStereo)
	{
		mpGraphicsState->setProgram(mpMonoSPSProgram);
		pContext->setGraphicsVars(mpMonoSPSVars);
	}
	else
	{
		mpGraphicsState->setProgram(mpStereoProgram);
		pContext->setGraphicsVars(mpStereoVars);
	}
	mpGraphicsState->setFbo(mpVrFbo->getFbo());
	pContext->pushGraphicsState(mpGraphicsState);

	// Render
	mpSceneRenderer->renderScene(pContext);

	// Restore the state
	pContext->popGraphicsState();

	// Submit the views and display them
	mpVrFbo->submitToHmd(pContext);
	blitTexture(pContext, pTargetFbo.get(), mpVrFbo->getEyeResourceView(VRDisplay::Eye::Left), 0);
	blitTexture(pContext, pTargetFbo.get(), mpVrFbo->getEyeResourceView(VRDisplay::Eye::Right), pTargetFbo->getWidth() / 2);
}

void StereoRendering::submitToScreen(RenderContext* pContext, Fbo::SharedPtr pTargetFbo)
{
	mpGraphicsState->setProgram(mpMonoSPSProgram);
	mpGraphicsState->setFbo(pTargetFbo);
	pContext->setGraphicsState(mpGraphicsState);
	pContext->setGraphicsVars(mpMonoSPSVars);
	mpSceneRenderer->renderScene(pContext);
}
*/


void VRDXR::setRenderMode()
{
	/*
	if (mpScene)
	{
		//mpMonoSPSProgram->removeDefine("_SINGLE_PASS_STEREO");

		mpGraphicsState->toggleSinglePassStereo(false);
		switch (mRenderMode)
		{
		case RenderMode::SinglePassStereo:
			//mpMonoSPSProgram->addDefine("_SINGLE_PASS_STEREO");
			mpGraphicsState->toggleSinglePassStereo(true);
			mpSceneRenderer->setCameraControllerType(SceneRenderer::CameraControllerType::Hmd);
			mpRtRenderer->setCameraControllerType(SceneRenderer::CameraControllerType::Hmd);
			break;
		case RenderMode::Stereo:
			mpSceneRenderer->setCameraControllerType(SceneRenderer::CameraControllerType::Hmd);
			mpRtRenderer->setCameraControllerType(SceneRenderer::CameraControllerType::Hmd);
			break;
		case RenderMode::Mono:
			mpSceneRenderer->setCameraControllerType(SceneRenderer::CameraControllerType::SixDof);
			mpRtRenderer->setCameraControllerType(SceneRenderer::CameraControllerType::SixDof);
			break;
		}
	}*/
}

void VRDXR::blitTexture(RenderContext* pContext, Fbo* pTargetFbo, Texture::SharedPtr pTexture, uint32_t xStart)
{
	if (mShowStereoViews)
	{
		uvec4 dstRect;
		dstRect.x = xStart;
		dstRect.y = 0;
		dstRect.z = xStart + (pTargetFbo->getWidth() / 2);
		dstRect.w = pTargetFbo->getHeight();
		pContext->blit(pTexture->getSRV(0, 1, 0, 1), pTargetFbo->getRenderTargetView(0), uvec4(-1), dstRect);
	}
}

CameraData VRDXR::calculateRightEyeParams() const
{
	// Save left eye view matrix.
	float4x4 leftView = mpCamera->getViewMatrix();
	
	// Overwrite view with right eye and calculate shader parameters.
	float4x4 rightView = mpCamera->getRightEyeViewMatrix();
	mpCamera->setViewMatrix(rightView);
	CameraData data = mpCamera->getData();
	
	// Restore left eye view matrix.
	mpCamera->setViewMatrix(leftView);

	return data;
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	VRDXR::UniquePtr pRenderer = std::make_unique<VRDXR>();
	SampleConfig config;
	config.windowDesc.title = "Stereo DXR";
	config.windowDesc.height = 1024;
	config.windowDesc.width = 1600;
	config.windowDesc.resizableWindow = true;
	config.deviceDesc.enableVR = true;

#ifdef _WIN32
	Sample::run(config, pRenderer);
#else
	config.argc = (uint32_t)argc;
	config.argv = argv;
	Sample::run(config, pRenderer);
#endif

	return 0;
}

/*
#ifdef _WIN32
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
#else
int main(int argc, char** argv)
#endif
{
	StereoRendering::UniquePtr pRenderer = std::make_unique<StereoRendering>();
	SampleConfig config;
	config.windowDesc.title = "Stereo Rendering";
	config.windowDesc.height = 1024;
	config.windowDesc.width = 1600;
	config.windowDesc.resizableWindow = true;
	config.deviceDesc.enableVR = true;
#ifdef FALCOR_VK
	config.deviceDesc.enableDebugLayer = false; // OpenVR requires an extension that the debug layer doesn't recognize. It causes the application to crash
#endif

#ifdef _WIN32
	Sample::run(config, pRenderer);
#else
	config.argc = (uint32_t)argc;
	config.argv = argv;
	Sample::run(config, pRenderer);
#endif
	return 0;
}
*/