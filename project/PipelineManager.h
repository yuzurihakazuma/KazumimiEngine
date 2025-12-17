#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <unordered_map>
#include "PipelineType.h"
#include "DirectXCommon.h"

using Microsoft::WRL::ComPtr;

// パイプラインマネージャー
class PipelineManager{
public:
	static PipelineManager* GetInstance(){
		static PipelineManager instance;
		return &instance;
	}

	void Initialize(DirectXCommon* dxCommon);

	void SetPipeline(
		ID3D12GraphicsCommandList* commandList,
		PipelineType type
	);

private:
	PipelineManager() = default;

	// 初期化処理
	void CreateSpriteRootSignature();
	void CreateSpriteGraphicsPipeline();

	void CreateObject3DRootSignature();
	void CreateObject3DGraphicsPipeline();


	DirectXCommon* dxCommon_ = nullptr;

	ComPtr<ID3D12RootSignature> spriteRootSignature_;
	ComPtr<ID3D12PipelineState> spritePipelineState_;

	
	ComPtr<ID3D12RootSignature> object3DRootSignature_;
	ComPtr<ID3D12PipelineState> object3DPipelineState_;

};

