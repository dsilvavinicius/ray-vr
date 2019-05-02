#ifndef CAMERA_ATTACHMENT
#define CAMERA_ATTACHMENT

#include "Falcor.h"

template< typename T >
class CameraAttachment
{
public:
	using SharedPtr = std::shared_ptr<CameraAttachment<T>>;
	
	static SharedPtr create(typename Falcor::ObjectInstance<T>::SharedPtr attachment) { return SharedPtr(new CameraAttachment(attachment)); }

	~CameraAttachment();
	
	void update(const mat4& view);

private:
	CameraAttachment(typename Falcor::ObjectInstance<T>::SharedPtr attachment);
	
	typename Falcor::ObjectInstance<T>::SharedPtr mAttachment;
	mat4 mOriginalTransform; // The original transformation of the attached object. It is restored when the attachment ends.
};

template< typename T >
CameraAttachment< T >::CameraAttachment(typename Falcor::ObjectInstance<T>::SharedPtr attachment)
	: mOriginalTransform(attachment->getTransformMatrix()),
	mAttachment(attachment)
{};

template< typename T >
CameraAttachment< T >::~CameraAttachment()
{
	update(mOriginalTransform);
}

template< typename T >
void CameraAttachment<T>::update(const mat4& view)
{
	float3 up(view[0][1], view[1][1], view[2][1]);
	float3 target(view[0][2], view[1][2], view[2][2]);
	//float3 pos(-view[0][3], -view[1][3], -view[2][3]);
	float3 pos = glm::inverse(view) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

	mAttachment->move(pos, pos + target, up);
}

#endif
