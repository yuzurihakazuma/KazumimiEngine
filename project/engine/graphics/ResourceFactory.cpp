#include "ResourceFactory.h"

// --- 標準ライブラリ ---
#include <cassert>

ResourceFactory* ResourceFactory::GetInstance(){
	static ResourceFactory instance;
	return &instance;
}


// 指定されたバイトサイズのバッファリソース（Upload Heap）を生成する
Microsoft::WRL::ComPtr<ID3D12Resource> ResourceFactory::CreateBufferResource(uint64_t sizeInBytes){
	// デバイスがセットされているか確認
	if ( device_ == nullptr ) {

		assert(false && "ResourceFactory::device_ is nullptr!");
		return nullptr;
	}

	// Upload Heap 用のヒーププロパティを設定
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; // Upload Heap を指定
	// バッファリソースのディスクリプションを設定
	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; // バッファリソースを指定
	desc.Width = sizeInBytes; // バイトサイズを幅に設定
	desc.Height = 1; // バッファリソースは高さを1に設定
	desc.DepthOrArraySize = 1; // バッファリソースは配列サイズを1に設定
	desc.MipLevels = 1; // バッファリソースはミップマップレベルを1に設定
	desc.SampleDesc.Count = 1; // バッファリソースはサンプル数を1に設定
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;// バッファリソースは行優先レイアウトを指定
	// リソースを生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
	HRESULT hr = device_->CreateCommittedResource(
		&heapProps, // Upload Heap を指定
		D3D12_HEAP_FLAG_NONE, // ヒープフラグはなし
		&desc,// バッファリソースのディスクリプションを指定
		D3D12_RESOURCE_STATE_GENERIC_READ,// バッファリソースはジェネリックリード状態で生成
		nullptr,// 最適化されたクリア値は不要なので nullptr を指定
		IID_PPV_ARGS(&resource)
	);
	// 生成に成功したか確認
	assert(SUCCEEDED(hr));
	return resource;
}


// 指定された幅・高さ・フォーマットのテクスチャリソース（Default Heap）を生成する
Microsoft::WRL::ComPtr<ID3D12Resource> ResourceFactory::CreateRenderTextureResource(uint32_t width, uint32_t height, DXGI_FORMAT format, const Vector4& clearColor){

	// デバイスがセットされているか確認
	if ( device_ == nullptr ) {
		assert(false && "ResourceFactory::device_ is nullptr!");
		return nullptr;
	}


	// デバイスがセットされているか確認
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT; // Default Heap を指定


	D3D12_RESOURCE_DESC desc = {}; // テクスチャリソースのディスクリプションを設定
	desc.Width = width; // テクスチャの幅を設定
	desc.Height = height; // テクスチャの高さを設定
	desc.MipLevels = 1; // ミップマップレベルを1に設定
	desc.DepthOrArraySize = 1; // 3Dテクスチャでない場合は1を設定
	desc.Format = format; // テクスチャのフォーマットを設定
	desc.SampleDesc.Count = 1; // サンプル数を1に設定
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2Dテクスチャを指定
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; // レンダーターゲットとして使用できるようにフラグを設定

	// クリア値を設定
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = format; // フォーマットを設定
	clearValue.Color[0] = clearColor.x; // クリアカラーのR成分を設定
	clearValue.Color[1] = clearColor.y; // クリアカラーのG成分を設定
	clearValue.Color[2] = clearColor.z; // クリアカラーのB成分を設定
	clearValue.Color[3] = clearColor.w; // クリアカラーのA成分を設定

	// リソースを生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;

	HRESULT hr = device_->CreateCommittedResource(
		&heapProps, // Default Heap を指定
		D3D12_HEAP_FLAG_NONE, // ヒープフラグはなし
		&desc, // テクスチャリソースのディスクリプションを指定
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 
		&clearValue, // クリア値を指定
		IID_PPV_ARGS(&resource)
	);
	// 生成に成功したか確認
	assert(SUCCEEDED(hr));
	// 生成したリソースを返す
	return resource;

}



void ResourceFactory::Finalize(){

	device_.Reset();
}