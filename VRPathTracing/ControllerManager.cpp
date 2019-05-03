#include "ControllerManager.h"
#include <FalcorExperimental.h>
#include <sstream>


ControllerManager::ControllerManager(Scene::SharedPtr scene)
{
	RtModel::SharedPtr leftHandModel =  RtModel::createFromFile("Avatar/left_hand.fbx", RtBuildFlags::None, Model::LoadFlags::BuffersAsShaderResource);
	RtModel::SharedPtr rightHandModel = RtModel::createFromFile("Avatar/right_hand.fbx", RtBuildFlags::None, Model::LoadFlags::BuffersAsShaderResource);

	if(leftHandModel)
	{
		scene->addModelInstance(leftHandModel, "Left Hand");
		mLeftHand = leftHandModel->getMeshInstance(0, 0);
	}
	
	if (rightHandModel)
	{
		scene->addModelInstance(rightHandModel, "Right Hand");
		mRightHand = rightHandModel->getMeshInstance(0, 0);
	}
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
		float3 up(world[0][1], world[1][1], world[2][1]);
		float3 target(world[0][2], world[1][2], world[2][2]);
		float3 pos = controller->getControllerCenter();

		hand->move(pos, pos + target, up);
	}
}
