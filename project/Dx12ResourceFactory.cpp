#include "Dx12ResourceFactory.h"


ComPtr<ID3D12Resource> Dx12ResourceFactory::CreateBufferResource(size_t sizeInBytes){

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = nullptr;
	// DXGIファクトリー
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr;
	// HRESULTはWindows系のエラーコードであり、
	// 関数が成功したかどうかをSUCCEDEDマクロで判定できる
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	// 頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties {};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;// UploadHeapを使う
	// 頂点リソースの設定
	D3D12_RESOURCE_DESC vertexResourcceDesc {};
	// バッファリソース。テクスチャの場合はまた別の設定をする
	vertexResourcceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourcceDesc.Width = sizeInBytes;// リソースのサイズ。今回はVector4を3頂点分
	// バッファの場合はこれらは1にする決まり
	vertexResourcceDesc.Height = 1;
	vertexResourcceDesc.DepthOrArraySize = 1;
	vertexResourcceDesc.MipLevels = 1;
	vertexResourcceDesc.SampleDesc.Count = 1;
	// バッファの場合はこれにする決まり
	vertexResourcceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	vertexResourcceDesc.Format = DXGI_FORMAT_UNKNOWN;            // バッファなので通常 UNKNOWN
	vertexResourcceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;        // バッファ用なら通常フラグなし


	hr = device_->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
		&vertexResourcceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&vertexResource));

	return vertexResource;

}
