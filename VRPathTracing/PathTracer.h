/***************************************************************************
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
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
#pragma once
#include "Falcor.h"
#include "FalcorExperimental.h"
#include "Avatar.h"

using namespace Falcor;
using namespace std;

class PathTracer : public Renderer
{
public:
    void onLoad(SampleCallbacks* pSample, RenderContext* pRenderContext) override;
    void onFrameRender(SampleCallbacks* pSample, RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo) override;
    void onShutdown(SampleCallbacks* pSample) override;
    void onResizeSwapChain(SampleCallbacks* pSample, uint32_t width, uint32_t height) override;
    bool onKeyEvent(SampleCallbacks* pSample, const KeyboardEvent& keyEvent) override;
    bool onMouseEvent(SampleCallbacks* pSample, const MouseEvent& mouseEvent) override;
    void onDataReload(SampleCallbacks* pSample) override;
    void onGuiRender(SampleCallbacks* pSample, Gui* pGui) override;

private:
    void toggleCameraPathState();
	void createRenderGraph(SampleCallbacks* pCallbacks, RenderGraph::SharedPtr& outRenderGraph);
	void loadModel(SampleCallbacks* pCallbacks, const string& filename);

    bool mCameraPath = false;
	bool mInstancesVisible = true;
    RenderGraph::SharedPtr mpLeftEyeGraph;
	RenderGraph::SharedPtr mpRightEyeGraph;

	FirstPersonCameraController mCamController;
	CameraAttachment<Falcor::Mesh>::SharedPtr mCamAttachment;
	Avatar::UniquePtr mAvatar;
	Model::SharedPtr mBoundaryModel;
	uint mAttachedModel = 0;
	uint mAttachedMesh = 0;
	uint mAttachedInstance = 0;

	bool mUseHMD = true;
	bool mLeftEyeOnly = false;

	// VR
	void initVR(Fbo* pTargetFbo);
	void blitTexture(RenderContext* pContext, Fbo* pTargetFbo, Texture::SharedPtr pTexture, uint32_t xStart);
	pair<uint, mat4> setupCamera(const VRDisplay::Eye& eye);
	
	VrFbo::UniquePtr mpVrFbo;
	bool mShowStereoViews = true;
	vector< vector< uint > > mMaterialIds;
	uint mGlobalMaterialId = 0;

	enum SceneType
	{
		MirroredCube,
		Torus,
		SeifertWeber,
		MirroredDodecahedron,
		Poincare,
		Euclidean
	};
	// This member is used to auto-detect special scenes that have boundaries.
	SceneType mSceneType = Euclidean;
};
