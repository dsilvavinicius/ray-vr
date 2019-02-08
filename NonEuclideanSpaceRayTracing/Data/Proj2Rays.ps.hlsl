__import ShaderCommon;
__import Shading;
__import DefaultVS;

float4 main(VertexOut vOut) : SV_TARGET
{
	//return vOut.prevPosH;
	return float4(vOut.posW, 1.0f);
}