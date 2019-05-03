#pragma once

#include<Falcor.h>

using namespace Falcor;

class ControllerManager
{
public:
	using UniquePtr = std::unique_ptr<ControllerManager>;

	static UniquePtr create(Scene::SharedPtr scene) { return UniquePtr(new ControllerManager(scene)); }
	~ControllerManager() {}
	void update();

private:
	using MeshInstance = typename ObjectInstance<Mesh>::SharedPtr;

	ControllerManager(Scene::SharedPtr scene);
	void update(const VRController::SharedPtr& controller, const MeshInstance& hand);

	MeshInstance mLeftHand;
	MeshInstance mRightHand;
};

