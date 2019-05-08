#pragma once

#include "ControllerManager.h"
#include "CameraAttachment.h"

class Avatar
{
public:
	using UniquePtr = std::unique_ptr<Avatar>;

	static UniquePtr create(RtScene::SharedPtr& scene) { return UniquePtr(new Avatar(scene)); }
	~Avatar() {}

	void update(const mat4& cameraView);

	void toggleHead();
	void toggleHands();

	void renderUI(Gui* pGui, const char* uiGroup);

private:
	struct Head
	{
		bool enabled = true;
		Model::SharedPtr model;
		CameraAttachment<Mesh>::SharedPtr camAttachment;
	};

	Avatar(RtScene::SharedPtr& scene);

	RtScene::SharedPtr mScene;

	ControllerManager::UniquePtr mHands;
	Head mHead;
};

