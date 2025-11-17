#include "Dx12TextrueManager.h"




DirectX::ScratchImage Dx12TextrueManager::LoadTexture(const std::string& filePath){
	
	// テクスチャファイルを読み込んでプログラムで扱えるようにする
	DirectX::ScratchImage image {};
	std::wstring filePathw = logManager.ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathw.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	// ミップマップの作成
	DirectX::ScratchImage mipImages {};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);

	// ミップマップ付きのデータを返す
	return mipImages;


}

Microsoft::WRL::ComPtr<ID3D12Resource> Dx12TextrueManager::CreateTextureResource(const DirectX::TexMetadata& metadata){

	// metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc {};
	resourceDesc.Width = UINT(metadata.width); // Textrueの幅
	resourceDesc.Height = UINT(metadata.height); // Textrueの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels); // mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize); // 奥行きor配列Textrueの配列数
	resourceDesc.Format = metadata.format; //TextrueのFormat 
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension); // Textrueの次元数。普段使っているのは2次元
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;  
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;       



	// 利用するHeapの設定。非常に特殊な運用・
	D3D12_HEAP_PROPERTIES heapProperties {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // 細かい設定を行う

	// Resourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr_ = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr, IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr_));

	return resource;




}



Microsoft::WRL::ComPtr<ID3D12Resource> Dx12TextrueManager::UploadTextureData(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages, ID3D12GraphicsCommandList* commandList){

	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device_.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, static_cast< UINT >( subresources.size() ));
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = resourceBuffer_->CreateBufferResource(intermediateSize);

	UpdateSubresources(commandList, texture.Get(), intermediateResource.Get(), 0, 0, static_cast< UINT >( subresources.size() ), subresources.data());

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList->ResourceBarrier(1, &barrier);

	return intermediateResource;



}

Microsoft::WRL::ComPtr<ID3D12Resource> Dx12TextrueManager::CreateDepthStencilTextureResource( int32_t width, int32_t height){
	
	// 生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc {};
	resourceDesc.Width = width; // Textureの幅
	resourceDesc.Height = height; // Textureの高さ
	resourceDesc.MipLevels = 1; // mipmap
	resourceDesc.DepthOrArraySize = 1; // 奥行きor 配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; //DepthStencilとして使う通知


	// 利用するHrapの設定
	D3D12_HEAP_PROPERTIES heapProperties {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る

	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue {};
	depthClearValue.DepthStencil.Depth = 1.0f; // 1.0f(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマット。Resourceと合わせる

	// Resorceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties, // Heepの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし
		&resourceDesc, // Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値を書き込む状態にしておく
		&depthClearValue, //Clear最適値
		IID_PPV_ARGS(&resource)); // 作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));

	return resource;



}
