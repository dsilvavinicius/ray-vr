#include "ControllerManager.h"
#include <sstream>


ControllerManager::ControllerManager(RtScene::SharedPtr& scene)
{
	RtModel::SharedPtr leftHandModel =  RtModel::createFromFile("Avatar/left_hand.fbx", RtBuildFlags::None, Model::LoadFlags::BuffersAsShaderResource);
	RtModel::SharedPtr rightHandModel = RtModel::createFromFile("Avatar/right_hand.fbx", RtBuildFlags::None, Model::LoadFlags::BuffersAsShaderResource);

	mLeftHand = addModelToScene(scene, leftHandModel, "Left Hand", 0);
	mRightHand = addModelToScene(scene, rightHandModel, "Right Hand", 1);
}

ControllerManager::MeshInstance ControllerManager::addModelToScene(RtScene::SharedPtr& scene, RtModel::SharedPtr& model, const std::string& name, uint controllerIdx)
{
	MeshInstance instance;

	if (model)
	{
		scene->addModelInstance(model, name);
		instance = model->getMeshInstance(0, 0);
		VRController::SharedPtr controller = VRSystem::instance()->getController(controllerIdx);
		float3 aim(0.f, 1.f, 0.f);
		controller->setControllerAimPoint(aim);
	}

	return instance;
}

void ControllerManager::update()
{
	VRController::SharedPtr leftController = VRSystem::instance()->getController(0);
	VRController::SharedPtr rightController = VRSystem::instance()->getController(1);

	update(leftController, mLeftHand);
	update(rightController, mRightHand);
}

void ControllerManager::update(const VRController::SharedPtr& controller, const MeshInstance& hand)
{
	if (controller && hand)
	{
		mat4 world = controller->getToWorldMatrix();
		
		float3 up = controller->getControllerVector();
		float3 target = controller->getControllerAimPoint();
		float3 pos = controller->getControllerCenter();

		hand->move(pos, target, up);
	}
}
