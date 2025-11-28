#include "TextrueManager.h"




void TextrueManager::Initialize(ComPtr<ID3D12Device> device, DirectXCommon* dxCommon,ID3D12DescriptorHeap* srvHeap, UINT descriptorSize){
	this->device_ = device;
	this->dxCommon_ = dxCommon;
	this->srvHeap_ = srvHeap;
	this->descriptorSizeSRV_ = descriptorSize;

}

D3D12_GPU_DESCRIPTOR_HANDLE TextrueManager::LoadAndCreateSRV(const std::string& filePath, ID3D12GraphicsCommandList* commandList){
	/*if ( textureMap.count(filePath) ) {
		return dxCommon_->GetGPUDescriptorHandle(srvHeap_, descriptorSizeSRV_, currentIndex_ - 1);
	}*/

	DirectX::ScratchImage mipImages = LoadTexture(filePath);
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = CreateTextureResource(metadata);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediate = UploadTextureData(textureResource, mipImages, commandList);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = static_cast< UINT >( metadata.mipLevels );

	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = dxCommon_->GetCPUDescriptorHandle(srvHeap_, descriptorSizeSRV_, currentIndex_);
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = dxCommon_->GetGPUDescriptorHandle(srvHeap_, descriptorSizeSRV_, currentIndex_);

	device_->CreateShaderResourceView(textureResource.Get(), &srvDesc, handleCPU);

	textureMap[filePath] = textureResource;
	currentIndex_++;

	return handleGPU;
}

DirectX::ScratchImage TextrueManager::LoadTexture(const std::string& filePath){

	// 1. 画像ファイルを読み込む
	DirectX::ScratchImage image {};
	std::wstring filePathw = logManager.ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathw.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);

	// 読み込み失敗時のガード
	if ( FAILED(hr) ) {
		logManager.Log("Failed to load texture: " + filePath + "\n");
		assert(false);
		return image;
	}

	// 2. ミップマップ生成
	DirectX::ScratchImage mipImages {};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);

	// 失敗したら元の画像をそのまま使う
	if ( FAILED(hr) ) {
		// ★修正：ここでも圧縮解除を試みる
		// もし元の画像自体がBC7などの場合、ここでも変換が必要かもしれません。
		// ですが、まずは GenerateMipMaps が成功するケース（2の累乗画像）を救います。
		return std::move(image);
	}

	// ★★★ 追加部分：圧縮フォーマットなら展開する ★★★
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	if ( DirectX::IsCompressed(metadata.format) ) {
		DirectX::ScratchImage decompressedImage {};
		// BC7などを R8G8B8A8_UNORM_SRGB に変換
		hr = DirectX::Decompress(
			mipImages.GetImages(), mipImages.GetImageCount(),
			metadata, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, decompressedImage);

		if ( SUCCEEDED(hr) ) {
			return std::move(decompressedImage);
		}
	}
	// ★★★★★★★★★★★★★★★★★★★★★★★★★★★

	return std::move(mipImages);
}
Microsoft::WRL::ComPtr<ID3D12Resource> TextrueManager::CreateTextureResource(const DirectX::TexMetadata& metadata){

	// 1. metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc {};
	resourceDesc.Width = UINT(metadata.width); // Textureの幅
	resourceDesc.Height = UINT(metadata.height); // Textureの高さ

	// ★安全策1: ミップマップ数が0（自動計算）や異常値になっていないか補正
	// ミップマップ生成に失敗した画像は mipLevels=1 になっているはずですが、念のため。
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);
	if ( resourceDesc.MipLevels == 0 ) {
		resourceDesc.MipLevels = 1; // 0なら1に強制する
	}

	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
	resourceDesc.Format = metadata.format;
	resourceDesc.SampleDesc.Count = 1;

	// ★安全策2: 次元のキャストをやめて、明示的にTEXTURE2Dを指定する
	// (metadata.dimensionの値がズレている可能性を排除するため)
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Alignment = 0; // 0にしておくと自動で64KBアライメントにしてくれる

	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;


	// 2. 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COPY_DEST;

	// 3. Resourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr_ = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr, IID_PPV_ARGS(&resource));

	// ★安全策3: エラーチェックと詳細ログ
	if ( FAILED(hr_) ) {
		// ここで「どんな値で失敗したか」をログに出すことで、原因が一発でわかります
		std::string log = std::format(
			"CreateTextureResource Failed. \nWidth:{} Height:{} MipLevels:{} Format:{}\n",
			resourceDesc.Width, resourceDesc.Height, resourceDesc.MipLevels, static_cast<int>(resourceDesc.Format)
		);
		logManager.Log(log);

		// 止める
		assert(SUCCEEDED(hr_));
	}

	return resource;
}


Microsoft::WRL::ComPtr<ID3D12Resource> TextrueManager::UploadTextureData(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages, ID3D12GraphicsCommandList* commandList){

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

Microsoft::WRL::ComPtr<ID3D12Resource> TextrueManager::CreateDepthStencilTextureResource( int32_t width, int32_t height){
	
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
