#include "Avatar.h"
#include "FalcorExperimental.h"

Avatar::Avatar(RtScene::SharedPtr& scene)
	: mHands(ControllerManager::create(scene)),
	mScene(scene)
{
	mHead.model = RtModel::createFromFile("Avatar/suzanne.fbx", RtBuildFlags::None, Model::LoadFlags::BuffersAsShaderResource);

	toggleHead();
}

void Avatar::toggleHead()
{
	if (mHead.camAttachment)
	{
		// Disable case
		for (uint i = 0; i < mScene->getModelCount(); ++i)
		{
			const std::string& name = mScene->getModel(i)->getName();
			if (name.compare("suzanne_copy") == 0)
			{
				mScene->deleteModel(i);
				break;
			}
		}
		
		mHead.camAttachment = nullptr;
	}
	else
	{
		// Enable case
		if (mHead.model)
		{
			mScene->addModelInstance(mHead.model, "suzanne_copy");
			ObjectInstance<Mesh>::SharedPtr instance = mHead.model->getMeshInstance(0, 0);
			mHead.camAttachment = CameraAttachment<Mesh>::create(instance);
		}
	}
}

void Avatar::toggleHands()
{
	mHands->toggle();
}

void Avatar::update(const mat4& cameraView)
{
	if(mHead.camAttachment)
	{
		mHead.camAttachment->update(cameraView);
	}
	mHands->update();
}

void Avatar::renderUI(Gui* pGui, const char* uiGroup)
{
	if (pGui->beginGroup(uiGroup))
	{
		if (pGui->addCheckBox("Head", mHead.enabled))
		{
			toggleHead();
		}
		if (pGui->addCheckBox("Hands", mHands->mEnabled))
		{
			toggleHands();
		}
		pGui->endGroup();
	}
}