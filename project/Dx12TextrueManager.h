#pragma once
#include <wrl.h>
#include "externals/DirectXTex/d3dx12.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "LogManager.h"
#include <ImGuiManager.h>

#include "Dx12ResourceFactory.h"



using Microsoft::WRL::ComPtr;

using namespace logs;

class Dx12TextrueManager{

public:

	Dx12TextrueManager(ComPtr<ID3D12Device> device)
		: device_(device){}

	DirectX::ScratchImage LoadTexture(const std::string& filePath);

	ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);

	[[nodiscard]] // 03_00EX
	ComPtr<ID3D12Resource> UploadTextureData(const ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages, 
		ID3D12GraphicsCommandList* commandList);
	
	ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(int32_t width, int32_t height);

	// device を外部からセットする関数（追加）
	void SetDevice(ComPtr<ID3D12Device> device){
		device_ = device;
	}

private:
	
	ComPtr<ID3D12Device> device_;

	LogManager logManager;

	
	Dx12ResourceFactory* resourceBuffer_;

};

