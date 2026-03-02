#include "GraphicsPipelineBuilder.h"
#include <cassert>

GraphicsPipelineBuilder::GraphicsPipelineBuilder(){
	//  よく使う標準設定をあらかじめ入れる
	psoDesc_.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	psoDesc_.NumRenderTargets = 1;
	psoDesc_.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc_.SampleDesc.Count = 1;

	// デフォルトラスタライザ（背面カリング、塗りつぶし）
	psoDesc_.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psoDesc_.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc_.RasterizerState.DepthClipEnable = true;

	// デフォルト深度ステンシル（深度テストあり、書き込みあり）
	psoDesc_.DepthStencilState.DepthEnable = true;
	psoDesc_.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	psoDesc_.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc_.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// デフォルトトポロジー
	psoDesc_.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetRootSignature(ID3D12RootSignature* rootSignature){
	psoDesc_.pRootSignature = rootSignature;
	return *this; 
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetShaders(IDxcBlob* vsBlob, IDxcBlob* psBlob){
	psoDesc_.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc_.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetInputLayout(D3D12_INPUT_ELEMENT_DESC* inputElements, UINT numElements){
	psoDesc_.InputLayout.pInputElementDescs = inputElements;
	psoDesc_.InputLayout.NumElements = numElements;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetInputLayoutEmpty(){
	psoDesc_.InputLayout.pInputElementDescs = nullptr;
	psoDesc_.InputLayout.NumElements = 0;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetBlendMode(BlendMode blendMode){
	psoDesc_.BlendState = GetBlendDesc(blendMode);
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType){
	psoDesc_.PrimitiveTopologyType = topologyType;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetCullMode(D3D12_CULL_MODE cullMode, D3D12_FILL_MODE fillMode){
	psoDesc_.RasterizerState.CullMode = cullMode;
	psoDesc_.RasterizerState.FillMode = fillMode;
	return *this;
}

GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetDepthStencil(bool isDepthEnable, bool isDepthWrite){
	psoDesc_.DepthStencilState.DepthEnable = isDepthEnable;
	psoDesc_.DepthStencilState.DepthWriteMask = isDepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	// 深度を使わない時はフォーマットをUNKNOWNにする
	psoDesc_.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	return *this;
}

void GraphicsPipelineBuilder::Build(ID3D12Device* device, Microsoft::WRL::ComPtr<ID3D12PipelineState>& outPipelineState){
	HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc_, IID_PPV_ARGS(&outPipelineState));
	assert(SUCCEEDED(hr));
}

// PipelineManager から移動してきた BlendMode 取得関数
D3D12_BLEND_DESC GraphicsPipelineBuilder::GetBlendDesc(BlendMode blendMode){
	D3D12_BLEND_DESC blendDesc {};
	// ... (今まで PipelineManager に書いていた中身をそのまま移す) ...
	// ※とりあえず最初は kNormal だけでも大丈夫です
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	// 必要に応じて blendMode で分岐
	return blendDesc;
}