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

private:
	Avatar(RtScene::SharedPtr& scene);

	ControllerManager::UniquePtr mHands;
	CameraAttachment<Mesh>::SharedPtr mHead;
};

