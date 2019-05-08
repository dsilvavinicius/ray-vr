#include "ControllerManager.h"
#include <sstream>


ControllerManager::ControllerManager(RtScene::SharedPtr& scene)
	: mScene(scene)
{
	mLeftHand.controllerIdx = 0;
	mLeftHand.name = "left_hand_copy";
	mLeftHand.model = RtModel::createFromFile("Avatar/left_hand.fbx", RtBuildFlags::None, Model::LoadFlags::BuffersAsShaderResource);
	
	
	mRightHand.controllerIdx = 1;
	mRightHand.name = "right_hand_copy";
	mRightHand.model = RtModel::createFromFile("Avatar/right_hand.fbx", RtBuildFlags::None, Model::LoadFlags::BuffersAsShaderResource);

	toggle();
}

void ControllerManager::toggle()
{
	toggle(mLeftHand);
	toggle(mRightHand);
}

void ControllerManager::toggle(Hand& hand)
{
	if (hand.instance)
	{
		// Disable case
		for (uint i = 0; i < mScene->getModelCount(); ++i)
		{
			if (mScene->getModel(i)->getName().compare(hand.name) == 0)
			{
				mScene->deleteModelInstance(i, 0);
				mScene->deleteModel(i);
				break;
			}
		}
		hand.instance = nullptr;
	}
	else
	{
		if (hand.model)
		{
			mScene->addModelInstance(hand.model, hand.name);
			hand.instance = hand.model->getMeshInstance(0, 0);
			VRController::SharedPtr controller = VRSystem::instance()->getController(hand.controllerIdx);
			float3 aim(0.f, 1.f, 0.f);
			controller->setControllerAimPoint(aim);
		}
	}
}

void ControllerManager::update()
{
	VRController::SharedPtr leftController = VRSystem::instance()->getController(mLeftHand.controllerIdx);
	VRController::SharedPtr rightController = VRSystem::instance()->getController(mRightHand.controllerIdx);

	update(leftController, mLeftHand.instance);
	update(rightController, mRightHand.instance);
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
