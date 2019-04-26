#ifndef CAMERA_ATTACHMENT
#define CAMERA_ATTACHMENT

#include "Falcor.h"

template< typename T >
class CameraAttachment
{
public:
	using SharedPtr = std::shared_ptr<CameraAttachment<T>>;
	
	static SharedPtr create(Falcor::Camera::SharedConstPtr camera, typename Falcor::ObjectInstance<T>::SharedPtr attachment) { return SharedPtr(new CameraAttachment(camera, attachment)); }

	~CameraAttachment();
	
	void update();

private:
	CameraAttachment(Falcor::Camera::SharedConstPtr camera, typename Falcor::ObjectInstance<T>::SharedPtr attachment);
	void update(float3 translation, float3 up, float3 target);
	Falcor::Camera::SharedConstPtr mCamera;
	typename Falcor::ObjectInstance<T>::SharedPtr mAttachment;

	mat4 mOriginalTransform; // The original transformation of the attached object. It is restored when the attachment ends.
};

template< typename T >
CameraAttachment< T >::CameraAttachment(Falcor::Camera::SharedConstPtr camera, typename Falcor::ObjectInstance<T>::SharedPtr attachment)
	: mOriginalTransform(attachment->getTransformMatrix()),
	mCamera(camera),
	mAttachment(attachment)
{};

template< typename T >
CameraAttachment< T >::~CameraAttachment()
{
	float3 pos(mOriginalTransform[0][3], mOriginalTransform[1][3], mOriginalTransform[2][3]);
	float3 up(mOriginalTransform[0][1], mOriginalTransform[1][1], mOriginalTransform[2][1]);
	float3 target(mOriginalTransform[0][2], mOriginalTransform[1][2], mOriginalTransform[2][2]);
	
	update(pos, up, target);
}

template< typename T >
void CameraAttachment<T>::update()
{
	const mat4& view = mCamera->getViewMatrix();
	float3 up(view[0][1], view[1][1], view[2][1]);
	float3 target(view[0][2], view[1][2], view[2][2]);
	
	update(mCamera->getPosition() + target * 1.0f, up, target);
}

template< typename T >
void CameraAttachment<T>::update(float3 translation, float3 up, float3 target)
{
	mAttachment->setTranslation(translation, false);
	mAttachment->setUpVector(up);
	mAttachment->setTarget(translation + target);
}

#endif
