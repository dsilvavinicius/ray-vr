#pragma once
#include "Falcor.h"
#include "FalcorExperimental.h"

using namespace Falcor;

class Blur :
	public RenderPass, inherit_shared_from_this<RenderPass, Blur>
{
public:
	using SharedPtr = std::shared_ptr<Blur>;

	static SharedPtr create(const Dictionary& params = {});
	
	virtual std::string getDesc() override { return "Anti-aliasing using Gaussian Blur."; }
	
	virtual RenderPassReflection reflect(void) const override;

	virtual void execute(RenderContext* pContext, const RenderData* pRenderData) override;

	virtual void renderUI(Gui* pGui, const char* uiGroup) override;

	~Blur() {}

private:
	Blur(uint32_t kernelSize, float sigma) : RenderPass("Blur"), mGaussian(GaussianBlur::create(kernelSize, sigma)) {}

	GaussianBlur::UniquePtr mGaussian;
	int mDoBlur;
};

