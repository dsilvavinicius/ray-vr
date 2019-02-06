__import ShaderCommon;
__import Shading;
__import DefaultVS;

float4 main(VertexOut vOut) : SV_TARGET
{
	//float4 rayDirection = float4(normalize(vOut.posW - gCamera.posW), 1);
	//return rayDirection;
	return vOut.prevPosH;
}