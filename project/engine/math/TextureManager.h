#pragma once
#include <wrl.h>
#include "externals/DirectXTex/d3dx12.h"
#include "externals/DirectXTex/DirectXTex.h"
#include "LogManager.h"
#include <ImGuiManager.h>
#include "SrvManager.h" 
#include "ResourceFactory.h"
#include <d3d12.h>
#include"DirectXCommon.h"
#include <unordered_map>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

using namespace logs;

class DirectXCommon;

struct TextureData{
    
    
    
    
    ComPtr<ID3D12Resource> resource;
    uint32_t srvIndex;
};

class TextureManager{

public:

	/// <summary>
	/// 初期化
	// </summary>
    void Initialize(ComPtr<ID3D12Device> device,DirectXCommon* dxCommon,SrvManager* srvManager);



    D3D12_GPU_DESCRIPTOR_HANDLE LoadAndCreateSRV(const std::string& filePath, ID3D12GraphicsCommandList* commandList);



    // -------------------- テクスチャ読み込み --------------------

       /// <summary>
       /// 画像ファイルを読み込み、ScratchImage 形式で返す
       /// </summary>
    DirectX::ScratchImage LoadTexture(const std::string& filePath);


    // -------------------- GPU テクスチャリソース生成 --------------------

    /// <summary>
    /// TexMetadata をもとに、GPU テクスチャリソース(Resource)を生成
    /// </summary>
    ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);


    // -------------------- テクスチャデータ転送 --------------------

    /// <summary>
    /// ScratchImage 内のピクセルデータを GPU テクスチャへアップロード
    /// </summary>
    [[nodiscard]]
    ComPtr<ID3D12Resource> UploadTextureData(
        const ComPtr<ID3D12Resource>& texture,
        const DirectX::ScratchImage& mipImages,
        ID3D12GraphicsCommandList* commandList
    );


    // -------------------- 深度ステンシルテクスチャ生成 --------------------

    /// <summary>
    /// 深度ステンシル用の GPU テクスチャリソースを生成
    /// </summary>
    ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(int32_t width, int32_t height);


    // -------------------- デバイス・依存コンポーネント設定 --------------------

    /// <summary>
    /// DirectX12 Device を設定する
    /// </summary>
    void SetDevice(ComPtr<ID3D12Device> device){
        device_ = device;
    }

    /// <summary>
    /// 外部のリソースファクトリをセット
    /// </summary>
    void SetResourceFactory(ResourceFactory* resourceBuffer){
        resourceBuffer_ = resourceBuffer;
    }

  /*  /// ディスクリプタサイズを手動で設定する
    void SetDescriptorSize(UINT size){
        this->descriptorSizeSRV_ = size;
    }*/

private:

    // -------------------- 内部リソース --------------------

    ComPtr<ID3D12Device> device_ = nullptr;   // DX12 デバイス
    LogManager logManager;                    // ログ出力用
    ResourceFactory* resourceBuffer_ = nullptr; // バッファ生成の補助クラス

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
   /* ID3D12DescriptorHeap* srvHeap_ = nullptr;
    UINT descriptorSizeSRV_ = 0;
    UINT currentIndex_ = 1;*/



    std::unordered_map<std::string, ComPtr<ID3D12Resource>> textureMap;

    std::unordered_map<std::string, TextureData> textureDatas;

    std::vector<ComPtr<ID3D12Resource>> intermediateResources_;

};

