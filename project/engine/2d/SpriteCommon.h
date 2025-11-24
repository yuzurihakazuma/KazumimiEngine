#pragma once
#include <wrl.h>
#include <d3d12.h>
#include"DirectXCommon.h"
#include "LogManager.h"
#include"ShaderCompiler.h"
#include "DirectXCommon.h"

using namespace logs;



class SpriteCommon{
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(DirectXCommon* dxCommon);
	// 共通の描画設定
	void PreDraw(ID3D12GraphicsCommandList* commandList);

	// Getter（必要に応じて）
	ID3D12RootSignature* GetRootSignature() const{ return rootSignature_.Get(); }
	ID3D12PipelineState* GetPipelineState() const{ return pipelineState_.Get(); }
	DirectXCommon* GetDxCommon() const{ return dxCommon_; }


	void SetTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle){
		textureSrvHandleGPU_ = handle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetTextureHandle() const{
		return textureSrvHandleGPU_;
	}

private:


	// RootSignatureの作成
	void CreateRootSignature();

	// GraphicsPipelineの生成
	void CreateGraphicsPipeline();


	DirectXCommon* dxCommon_ = nullptr; // 所有しない参照
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

	

	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU_ {};

};

