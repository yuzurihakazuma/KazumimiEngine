#include "SrvManager.h"
// -- - 標準ライブラリ-- -
#include <cassert>

// --- エンジン側のファイル ---
#include "engine/base/DirectXCommon.h"

SrvManager* SrvManager::GetInstance() {
	static SrvManager instance;
	return &instance;
}


void SrvManager::Initialize(DirectXCommon* dxCommon){

	this->dxCommon_ = dxCommon;
	// ディスクリプタサイズの取得
	descriptorHeaps_ = dxCommon_->CreateDescriptorHeap(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		kMaxSrvCount_,
		true
	);
	// デスクリプタサイズの取得
	descriptorSize_ = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	

}


uint32_t SrvManager::Allocate(){
	
	assert(useIndex_ < kMaxSrvCount_);
	
	// 使用中のインデックスを返してインクリメント
	int index = useIndex_;
	// Maxを超えたらエラー
	useIndex_++;
	
	return index;// 戻り値
}

D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index){

	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeaps_->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += ( descriptorSize_ * index );
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index){
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeaps_->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += ( descriptorSize_ * index );
	return handleGPU;
}

void SrvManager::CreateSRVforTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels){

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
	srvDesc.Format = Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = MipLevels;

	dxCommon_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex) 
	);

}

void SrvManager::CreateSRVforStructuredBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride){


	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = numElements;
	srvDesc.Buffer.StructureByteStride = structureByteStride;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	dxCommon_->GetDevice()->CreateShaderResourceView(
		pResource,
		&srvDesc,
		GetCPUDescriptorHandle(srvIndex));



}


void SrvManager::PreDraw(){
		// SRVヒープの設定
	ID3D12DescriptorHeap* descriptorHeap[] = {descriptorHeaps_.Get()};
	dxCommon_->GetCommandList()->SetDescriptorHeaps(1, descriptorHeap);
}

void SrvManager::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex){

		dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(
			RootParameterIndex,
			GetGPUDescriptorHandle(srvIndex));
}


