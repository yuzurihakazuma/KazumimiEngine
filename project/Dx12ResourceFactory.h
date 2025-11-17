#pragma once
#include <wrl.h>
#include "externals/DirectXTex/d3dx12.h"
#include "externals/DirectXTex/DirectXTex.h"
#include <dxgi1_6.h>



using Microsoft::WRL::ComPtr;


class Dx12ResourceFactory{
public:

   


    //DirectX12 のリソース生成を担当
    ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);

    // device を外部からセットする関数（追加）
    void SetDevice(ComPtr<ID3D12Device> device){
        device_ = device;
    }


    // 必要なら SRV 用テクスチャや RTV 用バッファなどもここに追加できる
private:
    
    ComPtr<ID3D12Device> device_;
};

