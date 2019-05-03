#pragma once

#include<Falcor.h>
#include <FalcorExperimental.h>

using namespace Falcor;

class ControllerManager
{
public:
	using UniquePtr = std::unique_ptr<ControllerManager>;
	using MeshInstance = typename ObjectInstance<Mesh>::SharedPtr;

	static UniquePtr create(RtScene::SharedPtr& scene) { return UniquePtr(new ControllerManager(scene)); }
	~ControllerManager() {}
	void update();

private:
	ControllerManager(RtScene::SharedPtr& scene);
	MeshInstance addModelToScene(RtScene::SharedPtr& scene, RtModel::SharedPtr& model, const std::string& name, uint controllerIdx);
	void update(const VRController::SharedPtr& controller, const MeshInstance& hand);

	MeshInstance mLeftHand;
	MeshInstance mRightHand;
};

