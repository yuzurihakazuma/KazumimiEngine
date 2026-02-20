#pragma once
#include <wrl.h>
#include "externals/DirectXTex/d3dx12.h"
#include "externals/DirectXTex/DirectXTex.h"
#include <dxgi1_6.h>



using Microsoft::WRL::ComPtr;


class ResourceFactory{
public:

    static ResourceFactory* GetInstance();

    // -------------------- リソース生成 --------------------

    /// <summary>
    /// 指定されたバイトサイズのバッファリソース（Upload Heap）を生成する
    /// </summary>
    ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);


    // -------------------- デバイス設定 --------------------

    /// <summary>
    /// 外部から D3D12 デバイスを設定する
    /// </summary>
    void SetDevice(ComPtr<ID3D12Device> device){
        this->device_ = device;
    }


    // -------------------- その他拡張予定 --------------------

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

