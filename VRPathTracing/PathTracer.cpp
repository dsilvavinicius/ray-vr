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
#include "RenderPasses/Blur.h"
#include "Data/ShaderTypes.h"
#include <sstream>

namespace
{
	enum SceneCamera
	{
		Default,
		Hmd,
		Fps
	};
}

void PathTracer::onGuiRender(SampleCallbacks* pCallbacks, Gui* pGui)
{
	pGui->addSeparator();

    if (pGui->addButton("Load Scene"))
    {
        assert(mpLeftEyeGraph != nullptr);
        std::string filename;
        if (openFileDialog(Scene::kFileExtensionFilters, filename))
        {
			loadModel(pCallbacks, filename);
        }
    }

	Scene::SharedPtr scene = mpLeftEyeGraph->getScene();

	scene->renderUI(pGui, "Scene");

	pGui->addSeparator();

	mAvatar->renderUI(pGui, "Avatar");

	pGui->addSeparator();

	if (mSceneType == Torus || mSceneType == Dodecahedron)
	{
		string expectedName = (mSceneType == Torus) ? "six_mirrors_room_only_edges_copy" : "dodecahedron_edges_copy";

		if (pGui->addButton("Toggle Boundaries"))
		{
			bool found = false;
			for (uint i = 0; i < scene->getModelCount(); ++i)
			{
				Model::SharedPtr model = scene->getModel(i);
				if (model->getName().compare(expectedName) == 0)
				{
					found = true;
					mBoundaryModel = model;
					scene->deleteModel(i);
					break;
				}
			}

			if (!found)
			{
				vec3 scale = (mSceneType == Torus) ? vec3(0.2, 0.2, 0.2) : vec3(0.01, 0.01, 0.01);
				scene->addModelInstance(mBoundaryModel, expectedName, vec3(), vec3(), scale);
			}
		}
	}

	pGui->addCheckBox("Display VR FBO", mShowStereoViews);

	pGui->addCheckBox("Left Eye Only", mLeftEyeOnly);

	pGui->addCheckBox("Use HMD", mUseHMD);

	if (pGui->addCheckBox("Camera Path", mCameraPath))
	{
		toggleCameraPathState();
	}

	// Camera attachment. One mesh instance can be selected to be attached to the camera.
	if (pGui->beginGroup("Camera Attachment"))
	{
		Gui::DropdownList modelAttachmentDrop;
		modelAttachmentDrop.push_back({ 0, "None" });

		for (uint i = 0; i < scene->getModelCount(); ++i)
		{
			stringstream ss; ss << "Model " << i;
			modelAttachmentDrop.push_back({i + 1, ss.str().c_str()});
		}
		
		if (pGui->addDropdown("Model", modelAttachmentDrop, mAttachedModel))
		{
			mAttachedMesh = 0;
			mAttachedInstance = 0;
			mCamAttachment = nullptr;
		}
		
		Gui::DropdownList meshAttachmentDrop;
		meshAttachmentDrop.push_back({ 0, "None" });
		
		if (mAttachedModel > 0)
		{
			uint meshes = scene->getModel(mAttachedModel - 1)->getMeshCount();
			for (uint i = 0; i < meshes; ++i)
			{
				stringstream ss; ss << "Mesh " << i;
				meshAttachmentDrop.push_back({i+1, ss.str().c_str()});
			}

			if (pGui->addDropdown("Mesh", meshAttachmentDrop, mAttachedMesh))
			{
				mAttachedInstance = 0;
				mCamAttachment = nullptr;
			}

			Gui::DropdownList instanceAttachmentDrop;
			instanceAttachmentDrop.push_back({ 0, "None" });

			if (mAttachedMesh > 0)
			{
				uint instances = scene->getModel(mAttachedModel - 1)->getMeshInstanceCount(mAttachedMesh - 1);
				for (uint i = 0; i < instances; ++i)
				{
					stringstream ss; ss << "Instance " << i;
					instanceAttachmentDrop.push_back({ i + 1 , ss.str().c_str() });
				}

				if (pGui->addDropdown("Instance", instanceAttachmentDrop, mAttachedInstance))
				{
					if (mAttachedInstance != 0)
					{
						Camera::SharedPtr camera = scene->getCamera(SceneCamera::Default);
						ObjectInstance<Mesh>::SharedPtr instance = scene->getModel(mAttachedModel - 1)->getMeshInstance(mAttachedMesh - 1, mAttachedInstance - 1);
						scene->getPath(0)->detachObject(instance);
						mCamAttachment = CameraAttachment<Mesh>::create(instance);
					}
					else
					{
						mCamAttachment = nullptr;
					}
				}
			}
		}

		pGui->endGroup();
	}

	if (pGui->addCheckBox("Meshes Visible", mInstancesVisible))
	{
		for (uint i = 0; i < scene->getModelCount(); ++i)
		{
			Model::SharedPtr model = scene->getModel(i);
			for (uint j = 0; j < model->getMeshCount(); ++j)
			{
				model->getMeshInstance(j, 0)->setVisible(mInstancesVisible);
			}
		}
	}

	Gui::DropdownList dropList;
	dropList.push_back({ RASTER, "Raster" });
	dropList.push_back({ RASTER_PLUS_SHADOWS, "Raster plus Shadows" });
	dropList.push_back({ REFLECTIONS, "Reflections" });
	dropList.push_back({ PERFECT_MIRROR, "Perfect Mirror" });
	#ifdef PATH_TRACING_ENABLED
		dropList.push_back({ PATH_TRACING, "Path Tracing" });
	#endif
	#ifdef TORUS_ENABLED
		dropList.push_back({ TORUS, "Torus" });
	#endif
	#ifdef DODECAHEDRON_ENABLED
		dropList.push_back({ SEIFERT_WEBER_DODECAHEDRON, "Seifert-Weber Dodecahedron" });
		dropList.push_back({ MIRRORED_DODECAHEDRON, "Mirrored Dodecahedron" });
	#endif
	#ifdef POINCARE_ENABLED
		dropList.push_back({ POINCARE_DODECAHEDRON, "Poincare Dodecahedron" });
	#endif

	if (pGui->addDropdown("Global Material ID", dropList, mGlobalMaterialId))
	{
		for (uint i = 0; i < scene->getModelCount(); ++i)
		{
			Model::SharedPtr model = scene->getModel(i);

			for (uint j = 0; j < model->getMeshCount(); ++j)
			{
				mMaterialIds[i][j] = mGlobalMaterialId;
				model->getMesh(j)->getMaterial()->setID(mMaterialIds[i][j]);
			}
		}
	}

	pGui->addSeparator();

    if (mpLeftEyeGraph != nullptr)
    {
		if (pGui->beginGroup("Render Graphs"))
		{
			if (pGui->beginGroup("Left Eye"))
			{
				mpLeftEyeGraph->renderUI(pGui, nullptr);
				pGui->endGroup();
			}
			
			if (pGui->beginGroup("Right Eye"))
			{
				mpRightEyeGraph->renderUI(pGui, nullptr);
				pGui->endGroup();
			}
			
			pGui->endGroup();
		}
    }

	// Material IDs.
	if (pGui->beginGroup("Material IDs", true))
	{
		for (uint i = 0; i < scene->getModelCount(); ++i)
		{
			Model::SharedPtr model = scene->getModel(i);

			stringstream ss; ss << "Model " << i << ", instances: " << model->getInstanceCount();
			if (pGui->beginGroup(ss.str().c_str(), true))
			{
				for (uint j = 0; j < model->getMeshCount(); ++j)
				{
					stringstream ss; ss << "Mesh " << j << ", instances: " << model->getMeshInstanceCount(j);
					
					if (pGui->beginGroup(ss.str().c_str(), false))
					{
						if (pGui->addDropdown(ss.str().c_str(), dropList, mMaterialIds[i][j]))
						{
							model->getMesh(j)->getMaterial()->setID(mMaterialIds[i][j]);
						}
						
						auto instance = model->getMeshInstance(j, 0);
						
						if (pGui->addButton("Toggle Visibility"))
						{
							instance->setVisible(!instance->isVisible());
						}

						pGui->endGroup();
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
    Scene::SharedPtr pScene = mpLeftEyeGraph->getScene();
    if (pScene != nullptr && pScene->getPathCount() > 0)
    {
        if (mCameraPath)
        {
			mUseHMD = false;
			pScene->getPath(0)->attachObject(pScene->getCamera(SceneCamera::Fps));
        }
        else
        {
			pScene->getPath(0)->detachObject(pScene->getCamera(SceneCamera::Fps));
        }
    }
}

void PathTracer::loadModel(SampleCallbacks* pCallbacks, const string& filename)
{
	ProgressBar::SharedPtr pBar = ProgressBar::create("Loading Scene", 100);

	RtScene::SharedPtr pScene = RtScene::loadFromFile(filename);

	if (pScene != nullptr)
	{
		if (filename.find("fundamental_domain_cube") != string::npos)
		{
			mSceneType = Torus;
		}
		else if (filename.find("dodecahedron") != string::npos)
		{
			mSceneType = Dodecahedron;
		}
		else
		{
			mSceneType = Euclidean;
		}

		mAvatar = Avatar::create(pScene);

		Fbo::SharedPtr pFbo = pCallbacks->getCurrentFbo();
		pScene->setCamerasAspectRatio(float(pFbo->getWidth()) / float(pFbo->getHeight()));
		mpLeftEyeGraph->setScene(pScene);
		mpRightEyeGraph->setScene(pScene);
		initVR(pCallbacks->getCurrentFbo().get());

		// Init material ids.
		mMaterialIds = vector< vector< uint > >(pScene->getModelCount());
		for (uint i = 0; i < pScene->getModelCount(); ++i)
		{
			mMaterialIds[i] = vector< uint >(pScene->getModel(i)->getMeshCount());
			for (uint j = 0; j < mMaterialIds[i].size(); ++j)
			{
				uint id = 0;
				mMaterialIds[i][j] = id;
				pScene->getModel(i)->getMesh(j)->getMaterial()->setID(id);
			}
		}

		// Additional FPS and HMD cameras.
		pScene->addCamera(Camera::create());
		pScene->addCamera(Camera::create());
	}
}

void PathTracer::createRenderGraph(SampleCallbacks* pCallbacks, RenderGraph::SharedPtr& outRenderGraph)
{
	outRenderGraph = RenderGraph::create("Path Tracer");
	outRenderGraph->addPass(GBufferRaster::create(), "GBuffer");

	auto params = Dictionary();
	params["useBlackEnvMap"] = true;
	params["doAccumulation"] = false;

	auto pGIPass = GGXGlobalIllumination::create(params);

	outRenderGraph->addPass(pGIPass, "GlobalIllumination");
	outRenderGraph->addPass(TemporalAccumulation::create(params), "TemporalAccumulation");
	outRenderGraph->addPass(Blur::create(params), "Blur");
	outRenderGraph->addPass(ToneMapping::create(), "ToneMapping");

	outRenderGraph->addEdge("GBuffer.posW", "GlobalIllumination.posW");
	outRenderGraph->addEdge("GBuffer.normW", "GlobalIllumination.normW");
	outRenderGraph->addEdge("GBuffer.diffuseOpacity", "GlobalIllumination.diffuseOpacity");
	outRenderGraph->addEdge("GBuffer.specRough", "GlobalIllumination.specRough");
	outRenderGraph->addEdge("GBuffer.emissive", "GlobalIllumination.emissive");
	outRenderGraph->addEdge("GBuffer.matlExtra", "GlobalIllumination.matlExtra");

	outRenderGraph->addEdge("GlobalIllumination.output", "TemporalAccumulation.input");

	outRenderGraph->addEdge("TemporalAccumulation.output", "Blur.src");

	//outRenderGraph->markOutput("ToneMapping.dst");
	//outRenderGraph->markOutput("TemporalAccumulation.output");
	//outRenderGraph->markOutput("GlobalIllumination.output");
	outRenderGraph->markOutput("Blur.dst");

	// When GI pass changes, tell temporal accumulation to reset
	pGIPass->setPassChangedCB([&]() {(*outRenderGraph->getPassesDictionary())["_dirty"] = true; });

	// Initialize the graph's record of what the swapchain size is, for texture creation
	outRenderGraph->onResize(pCallbacks->getCurrentFbo().get());
}

void PathTracer::onLoad(SampleCallbacks* pCallbacks, RenderContext* pRenderContext)
{
	createRenderGraph(pCallbacks, mpLeftEyeGraph);
	createRenderGraph(pCallbacks, mpRightEyeGraph);

	loadModel(pCallbacks, "Arcade/Arcade.fscene");
	//mFpsCam->setViewMatrix(VRSystem::instance()->getHMD()->getWorldMatrix());
}

void PathTracer::onFrameRender(SampleCallbacks* pCallbacks, RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo)
{
	VRSystem::instance()->refresh();

    const glm::vec4 clearColor(0.38f, 0.52f, 0.10f, 1);
    pRenderContext->clearFbo(pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);

	Scene::SharedPtr scene = mpLeftEyeGraph->getScene();

    if (scene != nullptr)
    {
		pRenderContext->clearFbo(mpVrFbo->getFbo().get(), clearColor, 1.0f, 0, FboAttachmentType::All);
		
		/*bool hasCameraMoved = mCamMovement.any();
		(*mpLeftEyeGraph->getPassesDictionary())["_camMoved"] = hasCameraMoved;
		(*mpRightEyeGraph->getPassesDictionary())["_camMoved"] = hasCameraMoved;*/

		if (mUseHMD)
		{
			scene->setActiveCamera(SceneCamera::Hmd);
		}
		else
		{
			scene->setActiveCamera(SceneCamera::Fps);
		}

		scene->update(pCallbacks->getCurrentTime(), &mCamController);

		float3 camPos = scene->getActiveCamera()->getPosition();

		// Left eye
		pair<uint, mat4> camIdxAndWorldMat = setupCamera(VRDisplay::Eye::Left);
		uint camIdx = camIdxAndWorldMat.first;
		mat4 world = camIdxAndWorldMat.second;

		if (mCamAttachment)
		{
			mCamAttachment->update(world);
		}

		if (mAvatar)
		{
			mAvatar->update(world, camPos);
		}

		mpLeftEyeGraph->execute(pRenderContext);
		//pRenderContext->blit(mpLeftEyeGraph->getOutput("TemporalAccumulation.output")->getSRV(), mpVrFbo->getFbo()->getColorTexture(0)->getRTV(0, 0, 1));
		//pRenderContext->blit(mpLeftEyeGraph->getOutput("ToneMapping.dst")->getSRV(), mpVrFbo->getFbo()->getColorTexture(0)->getRTV(0, 0, 1));
		//pRenderContext->blit(mpLeftEyeGraph->getOutput("GlobalIllumination.output")->getSRV(), mpVrFbo->getFbo()->getColorTexture(0)->getRTV(0, 0, 1));
		pRenderContext->blit(mpLeftEyeGraph->getOutput("Blur.dst")->getSRV(), mpVrFbo->getFbo()->getColorTexture(0)->getRTV(0, 0, 1));
		
		if (!mLeftEyeOnly)
		{
			// Right eye
			scene->setActiveCamera(camIdx);
			setupCamera(VRDisplay::Eye::Right);
			mpRightEyeGraph->execute(pRenderContext);
			//pRenderContext->blit(mpRightEyeGraph->getOutput("TemporalAccumulation.output")->getSRV(), mpVrFbo->getFbo()->getColorTexture(0)->getRTV(0, 1, 1));
			//pRenderContext->blit(mpRightEyeGraph->getOutput("ToneMapping.dst")->getSRV(), mpVrFbo->getFbo()->getColorTexture(0)->getRTV(0, 1, 1));
			//pRenderContext->blit(mpRightEyeGraph->getOutput("GlobalIllumination.output")->getSRV(), mpVrFbo->getFbo()->getColorTexture(0)->getRTV(0, 1, 1));
			pRenderContext->blit(mpRightEyeGraph->getOutput("Blur.dst")->getSRV(), mpVrFbo->getFbo()->getColorTexture(0)->getRTV(0, 1, 1));
		}

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
	if (mpLeftEyeGraph->getScene() != nullptr)
	{
		handled = mpLeftEyeGraph->onKeyEvent(keyEvent);
		mpRightEyeGraph->onKeyEvent(keyEvent);
	}

	if (!handled)
	{
		handled = mCamController.onKeyEvent(keyEvent);
	}
    
	return handled;
}

bool PathTracer::onMouseEvent(SampleCallbacks* pCallbacks, const MouseEvent& mouseEvent)
{
    bool handled = false;
	if (mpLeftEyeGraph->getScene() != nullptr)
	{
		handled = mpLeftEyeGraph->onMouseEvent(mouseEvent);
		mpRightEyeGraph->onMouseEvent(mouseEvent);
	}

	if (!handled)
	{
		handled = mCamController.onMouseEvent(mouseEvent);
	}

	return handled;
}

void PathTracer::onDataReload(SampleCallbacks* pCallbacks)
{

}

void PathTracer::onResizeSwapChain(SampleCallbacks* pCallbacks, uint32_t width, uint32_t height)
{
    if (mpLeftEyeGraph)
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

		mpLeftEyeGraph->onResize(mpVrFbo->getFbo().get());
		mpRightEyeGraph->onResize(mpVrFbo->getFbo().get());
		if (mpLeftEyeGraph->getScene() != nullptr)
		{
			mpLeftEyeGraph->getScene()->setCamerasAspectRatio(float(mpVrFbo->getFbo()->getWidth()) / float(mpVrFbo->getFbo()->getHeight()));
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

pair<uint, mat4> PathTracer::setupCamera(const VRDisplay::Eye& eye)
{
	VRDisplay* pDisplay = VRSystem::instance()->getHMD().get();

	Scene::SharedPtr scene = mpLeftEyeGraph->getScene();
	
	uint camIdx = scene->getActiveCameraIndex();

	glm::mat4 world = scene->getActiveCamera()->getViewMatrix();

	if (mUseHMD)
	{
		// Get HMD world matrix and apply additional camera transformation.
		world = pDisplay->getWorldMatrix();
		// Use FPS cam position to offset HMD.
		world = glm::translate(world, -scene->getActiveCamera()->getPosition());
	}

	scene->setActiveCamera(SceneCamera::Default);
	Camera::SharedPtr defaultCam = scene->getActiveCamera();
	
	glm::mat4 view = pDisplay->getOffsetMatrix(eye) * world;
	defaultCam->setPosition(glm::inverse(view) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
	defaultCam->setViewMatrix(view);
	pDisplay->setDepthRange(0.01f, 1000.f);
	defaultCam->setProjectionMatrix(pDisplay->getProjectionMatrix(eye));

	return pair<uint, mat4>(camIdx, world);
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
