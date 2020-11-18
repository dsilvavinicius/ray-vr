#include "Blur.h"

Blur::SharedPtr Blur::create(const Dictionary& params)
{
	uint kernelSize =	(params.keyExists("kernelSize")) ? params["kernelSize"] : 5;
	float sigma =		(params.keyExists("sigma")) ? params["sigma"] : 2.5f;
	bool mDoBlur =		(params.keyExists("doBlur")) ? params["doBlur"] : false;

	SharedPtr blur(new Blur(kernelSize, sigma));
	
	return blur;
}

RenderPassReflection Blur::reflect(void) const
{
	RenderPassReflection r;
	r.addInput("src", "");
	r.addOutput("dst", "").format(ResourceFormat::RGBA32Float).bindFlags(Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess | Resource::BindFlags::RenderTarget);
	return r;
}

void Blur::execute(RenderContext* pContext, const RenderData* pData)
{
	Texture::SharedPtr pSrcTex = pData->getTexture("src");

	// Get our output buffer and clear it
	Texture::SharedPtr pDstTex = pData->getTexture("dst");

	if (mDoBlur)
	{
		pContext->clearUAV(pDstTex->getUAV().get(), vec4(0.0f, 0.0f, 0.0f, 1.0f));

		if (pSrcTex == nullptr || pDstTex == nullptr) return;

		Fbo::Desc fboDesc;
		fboDesc.setColorTarget(0, ResourceFormat::RGBA32Float);
		// TODO: The FBO should not be created here.
		Fbo::SharedPtr fbo = FboHelper::create2D(pSrcTex->getWidth(), pSrcTex->getHeight(), fboDesc);

		mGaussian->execute(pContext, pSrcTex, fbo);

		pContext->blit(fbo->getRenderTargetView(0)->getResource()->getSRV(), pDstTex->getRTV());
	}
	else
	{
		pContext->blit(pSrcTex->getSRV(), pDstTex->getRTV());
	}
}

void Blur::renderUI(Gui* pGui, const char* uiGroup)
{
	pGui->addCheckBox("Enable", mDoBlur);

	mGaussian->renderUI(pGui, uiGroup);
}