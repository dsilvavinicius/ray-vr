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
#include "PathTracer.h"
#include "RenderPasses/GGXGlobalIllumination.h"
#include "RenderPasses/GBufferRaster.h"
#include "RenderPasses/TemporalAccumulation.h"
#include <sstream>

void PathTracer::onGuiRender(SampleCallbacks* pCallbacks, Gui* pGui)
{
    if (pGui->addButton("Load Scene"))
    {
        assert(mpGraph != nullptr);
        std::string filename;
        if (openFileDialog(Scene::kFileExtensionFilters, filename))
        {
            ProgressBar::SharedPtr pBar = ProgressBar::create("Loading Scene", 100);

            RtScene::SharedPtr pScene = RtScene::loadFromFile(filename);
            if (pScene != nullptr)
            {
                Fbo::SharedPtr pFbo = pCallbacks->getCurrentFbo();
                pScene->setCamerasAspectRatio(float(pFbo->getWidth()) / float(pFbo->getHeight()));
                mpGraph->setScene(pScene);
            }
        }
    }

    pGui->addSeparator();

	pGui->addCheckBox("Display VR FBO", mShowStereoViews);

    if (pGui->addButton(mDisableCameraPath ? "Enable Camera Path" : "Disable Camera Path"))
    {
        toggleCameraPathState();
    }

    if (mpGraph != nullptr)
    {
        mpGraph->renderUI(pGui, nullptr);
    }

	// Material IDs.
	if (pGui->beginGroup("Material IDs", true))
	{
		Scene::SharedPtr scene = mpGraph->getScene();

		for (uint i = 0; i < scene->getModelCount(); ++i)
		{
			Model::SharedPtr model = scene->getModel(i);

			stringstream ss; ss << "Model " << i;
			if (pGui->beginGroup(ss.str().c_str(), true))
			{
				for (uint j = 0; j < model->getMeshCount(); ++j)
				{
					stringstream ss; ss << " Mesh " << j;
					if (pGui->addIntVar(ss.str().c_str(), mMaterialIds[i][j], 0, 3))
					{
						model->getMesh(j)->getMaterial()->setID(mMaterialIds[i][j]);
					}
				}

				pGui->endGroup();
			}
		}

		pGui->endGroup();
	}
}

void PathTracer::toggleCameraPathState()
{
    Scene::SharedPtr pScene = mpGraph->getScene();
    if (pScene != nullptr && pScene->getPathCount() > 0)
    {
        mDisableCameraPath = !mDisableCameraPath;
        if (mDisableCameraPath)
        {
            pScene->getPath(0)->detachObject(pScene->getActiveCamera());
        }
        else
        {
            pScene->getPath(0)->attachObject(pScene->getActiveCamera());
        }
    }
}

void PathTracer::onLoad(SampleCallbacks* pCallbacks, RenderContext* pRenderContext)
{
    mpGraph = RenderGraph::create("Path Tracer");
    mpGraph->addPass(GBufferRaster::create(), "GBuffer");
	
	auto GGXGIParams = Dictionary();
	GGXGIParams["useBlackEnvMap"] = true;
	auto pGIPass = GGXGlobalIllumination::create(GGXGIParams);
	
    mpGraph->addPass(pGIPass, "GlobalIllumination");
    mpGraph->addPass(TemporalAccumulation::create(), "TemporalAccumulation");
    mpGraph->addPass(ToneMapping::create(), "ToneMapping");

    mpGraph->addEdge("GBuffer.posW", "GlobalIllumination.posW");
    mpGraph->addEdge("GBuffer.normW", "GlobalIllumination.normW");
    mpGraph->addEdge("GBuffer.diffuseOpacity", "GlobalIllumination.diffuseOpacity");
    mpGraph->addEdge("GBuffer.specRough", "GlobalIllumination.specRough");
    mpGraph->addEdge("GBuffer.emissive", "GlobalIllumination.emissive");
    mpGraph->addEdge("GBuffer.matlExtra", "GlobalIllumination.matlExtra");

    //mpGraph->addEdge("GlobalIllumination.output", "TemporalAccumulation.input");

    //mpGraph->addEdge("TemporalAccumulation.output", "ToneMapping.src");

	//mpGraph->markOutput("ToneMapping.dst");
    mpGraph->markOutput("GlobalIllumination.output");

    // When GI pass changes, tell temporal accumulation to reset
    pGIPass->setPassChangedCB([this]() {(*mpGraph->getPassesDictionary())["_dirty"] = true; });

    // Initialize the graph's record of what the swapchain size is, for texture creation
    mpGraph->onResize(pCallbacks->getCurrentFbo().get());

    {
        ProgressBar::SharedPtr pBar = ProgressBar::create("Loading Scene", 100);

        RtScene::SharedPtr pScene = RtScene::loadFromFile("Arcade/Arcade.fscene");
		mpGraph->setScene(pScene);
		initVR(pCallbacks->getCurrentFbo().get());
		
		// Init material ids.
		mMaterialIds = vector< vector< int > >( pScene->getModelCount() );
		for (uint i = 0; i < pScene->getModelCount(); ++i)
		{
			mMaterialIds[i] = vector< int >( pScene->getModel(i)->getMeshCount() );
			for (uint j = 0; j < mMaterialIds[i].size(); ++j)
			{
				mMaterialIds[i][j] = 0;
				pScene->getModel(i)->getMesh(j)->getMaterial()->setID(0);
			}
		}
    }

	//Camera::SharedPtr camera = mpGraph->getScene()->getActiveCamera();
	//mCamController.attachCamera(camera);
}

void PathTracer::onFrameRender(SampleCallbacks* pCallbacks, RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo)
{
	VRSystem::instance()->refresh();

    const glm::vec4 clearColor(0.38f, 0.52f, 0.10f, 1);
    pRenderContext->clearFbo(pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);

    if (mpGraph->getScene() != nullptr)
    {
		pRenderContext->clearFbo(mpVrFbo->getFbo().get(), clearColor, 1.0f, 0, FboAttachmentType::All);

		mpGraph->getScene()->update(pCallbacks->getCurrentTime(), &mCamController);

		// Left eye
		setupCamera(VRDisplay::Eye::Left);
        mpGraph->execute(pRenderContext);
		//pRenderContext->blit(mpGraph->getOutput("ToneMapping.dst")->getSRV(), mpVrFbo->getFbo()->getColorTexture(0)->getRTV(0, 0, 1));
		pRenderContext->blit(mpGraph->getOutput("GlobalIllumination.output")->getSRV(), mpVrFbo->getFbo()->getColorTexture(0)->getRTV(0, 0, 1));
		
		// Right eye
		setupCamera(VRDisplay::Eye::Right);
		mpGraph->execute(pRenderContext);
		//pRenderContext->blit(mpGraph->getOutput("GlobalIllumination.output")->getSRV(), mpVrFbo->getFbo()->getColorTexture(0)->getRTV(0, 1, 1));
		pRenderContext->blit(mpGraph->getOutput("GlobalIllumination.output")->getSRV(), mpVrFbo->getFbo()->getColorTexture(0)->getRTV(0, 1, 1));

		mpVrFbo->submitToHmd(pRenderContext);
		blitTexture(pRenderContext, pTargetFbo.get(), mpVrFbo->getEyeResourceView(VRDisplay::Eye::Left), 0);
		blitTexture(pRenderContext, pTargetFbo.get(), mpVrFbo->getEyeResourceView(VRDisplay::Eye::Right), pTargetFbo->getWidth() / 2);
    }
}

void PathTracer::onShutdown(SampleCallbacks* pCallbacks)
{
}

bool PathTracer::onKeyEvent(SampleCallbacks* pCallbacks, const KeyboardEvent& keyEvent)
{
    if (keyEvent.key == KeyboardEvent::Key::Minus && keyEvent.type == KeyboardEvent::Type::KeyPressed)
    {
        toggleCameraPathState();
        return true;
    }

    bool handled = false;
    if (mpGraph->getScene() != nullptr) handled = mpGraph->onKeyEvent(keyEvent);
    return handled ? true : mCamController.onKeyEvent(keyEvent);
}

bool PathTracer::onMouseEvent(SampleCallbacks* pCallbacks, const MouseEvent& mouseEvent)
{
    bool handled = false;
    if (mpGraph->getScene() != nullptr) handled = mpGraph->onMouseEvent(mouseEvent);
    return handled ? true : mCamController.onMouseEvent(mouseEvent);
}

void PathTracer::onDataReload(SampleCallbacks* pCallbacks)
{

}

void PathTracer::onResizeSwapChain(SampleCallbacks* pCallbacks, uint32_t width, uint32_t height)
{
    if (mpGraph)
    {
		initVR(pCallbacks->getCurrentFbo().get());
    }
}

void PathTracer::initVR(Fbo* pTargetFbo)
{
	if (VRSystem::instance())
	{
		// Create the FBOs
		Fbo::Desc vrFboDesc;

		vrFboDesc.setColorTarget(0, pTargetFbo->getColorTexture(0)->getFormat());
		vrFboDesc.setDepthStencilTarget(pTargetFbo->getDepthStencilTexture()->getFormat());

		mpVrFbo = VrFbo::create(vrFboDesc);

		mpGraph->onResize(mpVrFbo->getFbo().get());
		if (mpGraph->getScene() != nullptr)
		{
			mpGraph->getScene()->setCamerasAspectRatio(float(mpVrFbo->getFbo()->getWidth()) / float(mpVrFbo->getFbo()->getHeight()));
		}
	}
	else
	{
		msgBox("Can't initialize the VR system. Make sure that your HMD is connected properly");
	}
}

void PathTracer::blitTexture(RenderContext* pContext, Fbo* pTargetFbo, Texture::SharedPtr pTexture, uint32_t xStart)
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

void PathTracer::setupCamera(const VRDisplay::Eye& eye)
{
	VRDisplay* pDisplay = VRSystem::instance()->getHMD().get();

	// Get HMD world matrix and apply additional camera transformations.
	glm::mat4 hmdW = pDisplay->getWorldMatrix();
	/*hmdW = glm::translate(hmdW, mCamTranslation);
	hmdW = glm::rotate(hmdW, mCamRotation.x, vec3(1.f, 0.f, 0.f));
	hmdW = glm::rotate(hmdW, mCamRotation.y, vec3(0.f, 1.f, 0.f));
	hmdW = glm::rotate(hmdW, mCamRotation.z, vec3(0.f, 0.f, 1.f));
	hmdW = glm::scale(hmdW, mCamZoom);*/

	Camera::SharedPtr camera = mpGraph->getScene()->getActiveCamera();

	glm::mat4 view = pDisplay->getOffsetMatrix(eye) * hmdW;
	camera->setPosition(glm::inverse(view) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	camera->setViewMatrix(view);
	camera->setProjectionMatrix(pDisplay->getProjectionMatrix(eye));
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    PathTracer::UniquePtr pRenderer = std::make_unique<PathTracer>();
    SampleConfig config;
    config.windowDesc.title = "VR Path Tracing";
    config.windowDesc.resizableWindow = true;
    config.freezeTimeOnStartup = true;
	config.deviceDesc.enableVR = true;

    Sample::run(config, pRenderer);
    return 0;
}
