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
    void SetResourceFactory(Dx12ResourceFactory* resourceBuffer){
        resourceBuffer_ = resourceBuffer;
    }


private:

    // -------------------- 内部リソース --------------------

    ComPtr<ID3D12Device> device_ = nullptr;   // DX12 デバイス
    LogManager logManager;                    // ログ出力用
    Dx12ResourceFactory* resourceBuffer_ = nullptr; // バッファ生成の補助クラス
};

