#pragma once
// --- 標準・外部ライブラリ ---
#include <cstdint>
#include <d3d12.h>
#include <dxgiformat.h>
#include <wrl.h>


// 前方宣言
class DirectXCommon;

// SRVマネージャー
class SrvManager{
public:
	static SrvManager* GetInstance();

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

	ID3D12DescriptorHeap* GetDescriptorHeap() const{
		return descriptorHeaps_.Get();
	}

private:


	// ダイレクトX
	DirectXCommon* dxCommon_ = nullptr;

	// 最大SRV数
	static constexpr  uint32_t kMaxSrvCount_ = 2048;

	// 現在のSRV数
	uint32_t descriptorSize_;
	// ディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeaps_;

	// 使用中のインデックス
	uint32_t useIndex_ = 0;
private:
	// コンストラクタを private にして外部からの new を禁止
	SrvManager() = default;
	~SrvManager() = default;
	SrvManager(const SrvManager&) = delete;
	SrvManager& operator=(const SrvManager&) = delete;

};

