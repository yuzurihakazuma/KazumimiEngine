#pragma once
#include <cstdint>     // uint32_t
#include <d3d12.h>     // D3D12_*
#include <dxgiformat.h>
#include <wrl.h>       // ComPtr
using Microsoft::WRL::ComPtr;

class DirectXCommon;

class SrvManager{
public:


	// 初期化
	void Initialize(DirectXCommon* dxCommon);

	// SRV用ディスクリプタヒープからディスクリプタを1つ割り当てる
	uint32_t Allocate();

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

	// 2Dテクスチャ用SRVの作成
	void CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);

	void CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

	void PreDraw();

	void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);

private:


	// ダイレクトX
	DirectXCommon* dxCommon_ = nullptr;

	// 最大SRV数
	static constexpr  uint32_t kMaxSrvCount_ = 2048;

	// 現在のSRV数
	uint32_t descriptorSize_;
	// ディスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> descriptorHeaps_;

	// 使用中のインデックス
	uint32_t useIndex_ = 0;


};

