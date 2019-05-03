#include "Avatar.h"
#include "FalcorExperimental.h"

Avatar::Avatar(const Scene::SharedPtr& scene)
	: mHands(ControllerManager::create(scene))
{
	RtModel::SharedPtr model = RtModel::createFromFile("Avatar/suzanne.fbx", RtBuildFlags::None, Model::LoadFlags::BuffersAsShaderResource);

	if (model)
	{
		scene->addModelInstance(model, "Head");
		ObjectInstance<Mesh>::SharedPtr instance = model->getMeshInstance(0, 0);
		mHead = CameraAttachment<Mesh>::create(instance);
	}
}

void Avatar::update(const mat4& cameraView)
{
	if(mHead)
	{
		mHead->update(cameraView);
	}
	mHands->update();
}