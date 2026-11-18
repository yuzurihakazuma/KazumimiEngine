#pragma once
// --- 標準・外部ライブラリ ---
#include <wrl.h>
#include "engine/base/Singleton.h"
#include <d3d12.h>
#include <cstdint>

// --- エンジン側のファイル ---
#include "engine/math/struct.h"

namespace KazumimiEngine {

using Microsoft::WRL::ComPtr;


// =====================================================================
//  ResourceFactory：GPU のリソース（頂点や定数のバッファ・描き込み先のテクスチャ・コンピュート用のバッファ）を作る窓口。
//   作り方（ヒープの種類・はじめの状態）をここに集めて、使う側は「大きさ」などを渡すだけにする
// =====================================================================
class ResourceFactory : public Singleton<ResourceFactory> {
	friend class Singleton<ResourceFactory>; // 土台（Singleton）だけが作れる
public:


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
private:

    // -------------------- 内部リソース --------------------

    ComPtr<ID3D12Device> device_ = nullptr;  // バッファ作成に使用する D3D12 デバイス
};

} // namespace KazumimiEngine
