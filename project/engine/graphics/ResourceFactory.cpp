#include "ResourceFactory.h"

// --- 標準ライブラリ ---
#include <cassert>

ResourceFactory* ResourceFactory::GetInstance() {
    static ResourceFactory instance;
    return &instance;
}


ComPtr<ID3D12Resource> ResourceFactory::CreateBufferResource(uint64_t sizeInBytes){
   
    if ( device_ == nullptr ) {
        
        assert(false && "ResourceFactory::device_ is nullptr!");
        return nullptr;
    }


    
    
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = sizeInBytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> resource;
    HRESULT hr = device_->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&resource)
    );

    assert(SUCCEEDED(hr));
    return resource;
}
