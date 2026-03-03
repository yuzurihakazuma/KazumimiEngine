#pragma once

// --- 標準・外部ライブラリ ---
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include "engine/graphics/BlendMode.h"
#include <dxcapi.h>


// グラフィックスパイプラインを簡単に構築するためのビルダー
class GraphicsPipelineBuilder{
public:
	// コンストラクタで「よく使うデフォルト設定」を済ませる
	GraphicsPipelineBuilder();

	// --- 各種設定関数（チェーン呼び出しできるように自身を参照で返す） ---
	GraphicsPipelineBuilder& SetRootSignature(ID3D12RootSignature* rootSignature);
	GraphicsPipelineBuilder& SetShaders(IDxcBlob* vsBlob, IDxcBlob* psBlob);
	GraphicsPipelineBuilder& SetInputLayout(D3D12_INPUT_ELEMENT_DESC* inputElements, UINT numElements);
	GraphicsPipelineBuilder& SetInputLayoutEmpty(); // 頂点を使わない（PostEffectなど）用
	GraphicsPipelineBuilder& SetBlendMode(BlendMode blendMode);
	GraphicsPipelineBuilder& SetTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE topologyType);
	GraphicsPipelineBuilder& SetCullMode(D3D12_CULL_MODE cullMode, D3D12_FILL_MODE fillMode = D3D12_FILL_MODE_SOLID);
	GraphicsPipelineBuilder& SetDepthStencil(bool isDepthEnable, bool isDepthWrite = true);

	// 最後にPSOを生成する
	void Build(ID3D12Device* device, Microsoft::WRL::ComPtr<ID3D12PipelineState>& outPipelineState);

private:
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc_ {}; // 構築中の設定データ

	// PipelineManagerにあったGetBlendDescをこっちに移動させるとスッキリします
	D3D12_BLEND_DESC GetBlendDesc(BlendMode blendMode);
};