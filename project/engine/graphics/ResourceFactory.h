#pragma once
// --- 標準・外部ライブラリ ---
#include <wrl.h>
#include <d3d12.h>
#include <cstdint>

// --- エンジン側のファイル ---
#include "engine/math/struct.h"

using Microsoft::WRL::ComPtr;


class ResourceFactory{
public:

    static ResourceFactory* GetInstance();

    // -------------------- リソース生成 --------------------

    /// <summary>
    /// 指定されたバイトサイズのバッファリソース（Upload Heap）を生成する
    /// </summary>
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

	/// <summary>
	/// 指定された幅・高さ・フォーマットのテクスチャリソース（Default Heap）を生成する
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateRenderTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor);

	/// <summary>
	/// 指定されたバイトサイズのバッファリソース（Default Heap）を生成する
	// UAV バッファは、GPU から書き込み可能なバッファで、Compute Shader や Pixel Shader などで使用されることが多いです。
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateUAVBuffer(size_t sizeInBytes);



    // -------------------- デバイス設定 --------------------

    /// <summary>
    /// 外部から D3D12 デバイスを設定する
    /// </summary>
    void SetDevice(ComPtr<ID3D12Device> device){
        this->device_ = device;
    }

	// -------------------- 終了処理 --------------------
    void Finalize();

private:
    // コンストラクタを private に
    ResourceFactory() = default;
    ~ResourceFactory() = default;
    ResourceFactory(const ResourceFactory&) = delete;
    ResourceFactory& operator=(const ResourceFactory&) = delete;
private:

    // -------------------- 内部リソース --------------------

    ComPtr<ID3D12Device> device_ = nullptr;  // バッファ作成に使用する D3D12 デバイス
};

