#include "TextureManager.h"




void TextureManager::Initialize(ComPtr<ID3D12Device> device, DirectXCommon* dxCommon,SrvManager* srvManager){
	this->device_ = device;
	this->dxCommon_ = dxCommon;
	this->srvManager_ = srvManager;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::LoadAndCreateSRV(const std::string& filePath, ID3D12GraphicsCommandList* commandList){
	DirectX::ScratchImage mipImages = LoadTexture(filePath);
	// ミップマップ生成失敗時のガード
	if ( mipImages.GetImageCount()==0 ){
		return D3D12_GPU_DESCRIPTOR_HANDLE { 0 }; // 空ハンドルで戻る
	}


	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = CreateTextureResource(metadata);

	// リソース生成失敗時は即座に安全に戻る
	if ( !textureResource ) {
		logManager.Log("LoadAndCreateSRV: CreateTextureResource returned nullptr. Skipping upload and SRV creation.\n");
		return D3D12_GPU_DESCRIPTOR_HANDLE{ 0 }; // 空ハンドルで戻る
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> intermediate = UploadTextureData(textureResource, mipImages, commandList);

	// 4. SrvManagerを使って場所(Index)を確保する
	uint32_t srvIndex = srvManager_->Allocate();

	
	// CreateTextureResourceで MipLevels=1 に固定しているので、ここでも 1 を指定
	srvManager_->CreateSRVforTexture2D(srvIndex, textureResource.Get(), metadata.format, 1);

	// 6. GPUハンドルを取得 (Spriteにセットするために必要)
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = srvManager_->GetGPUDescriptorHandle(srvIndex);


	/*D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = dxCommon_->GetCPUDescriptorHandle(srvHeap_, descriptorSizeSRV_, currentIndex_);
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = dxCommon_->GetGPUDescriptorHandle(srvHeap_, descriptorSizeSRV_, currentIndex_);

	device_->CreateShaderResourceView(textureResource.Get(), &srvDesc, handleCPU);*/

	textureMap[filePath] = textureResource;
	
	return handleGPU;
}

//DirectX::ScratchImage TextrueManager::LoadTexture(const std::string& filePath){
//
//	// 1. 画像ファイルを読み込む
//	DirectX::ScratchImage image {};
//	std::wstring filePathw = logManager.ConvertString(filePath);
//	HRESULT hr = DirectX::LoadFromWICFile(filePathw.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
//
//	// 読み込み失敗時のガード
//	if ( FAILED(hr) ) {
//		logManager.Log("Failed to load texture: " + filePath + "\n");
//		assert(false);
//		return image;
//	}
//
//	// 2. ミップマップ生成
//	DirectX::ScratchImage mipImages {};
//	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
//
//	// 失敗したら元の画像をそのまま使う
//	if ( FAILED(hr) ) {
//		
//		return std::move(image);
//	}
//
//	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
//
//	DirectX::ScratchImage convertedImage {};
//
//	// R8G8B8A8_UNORM_SRGB 以外のフォーマットなら変換する
//	if ( metadata.format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ) {
//		hr = DirectX::Convert(
//			mipImages.GetImages(), mipImages.GetImageCount(),
//			mipImages.GetMetadata(),
//			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, // このフォーマットに強制統一
//			DirectX::TEX_FILTER_SRGB,
//			0,
//			convertedImage
//		);
//
//		if ( SUCCEEDED(hr) ) {
//			return std::move(convertedImage); // 変換成功したらそっちを返す
//		}
//	}
//
//
//
//	if ( DirectX::IsCompressed(metadata.format) ) {
//		// BC7などを R8G8B8A8_UNORM_SRGB に変換
//		DirectX::ScratchImage decompressedImage {};
//		hr = DirectX::Decompress(
//			mipImages.GetImages(), mipImages.GetImageCount(),
//			metadata, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, decompressedImage);
//
//		if ( SUCCEEDED(hr) ) {
//			return std::move(decompressedImage);
//		}
//	}
//	
//
//	return std::move(mipImages);
//}

// TextrueManager.cpp

DirectX::ScratchImage TextureManager::LoadTexture(const std::string& filePath){

	// 1. 画像ファイルを読み込む
	DirectX::ScratchImage image {};
	std::wstring filePathw = logManager.ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathw.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);

	if ( FAILED(hr) ) {
		logManager.Log("Failed to load texture: " + filePath + "\n");
		assert(false);
		return image;
	}

	// 2. 圧縮されていたら展開
	if ( DirectX::IsCompressed(image.GetMetadata().format) ) {
		DirectX::ScratchImage decompressedImage {};
		hr = DirectX::Decompress(
			image.GetImages(), image.GetImageCount(),
			image.GetMetadata(), DXGI_FORMAT_UNKNOWN, decompressedImage);
		if ( SUCCEEDED(hr) ) {
			image = std::move(decompressedImage);
		}
	}

	// 3. Format:91 (BGRA) 問題を回避するため、RGBAに変換
	if ( image.GetMetadata().format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ) {
		DirectX::ScratchImage convertedImage {};
		hr = DirectX::Convert(
			image.GetImages(), image.GetImageCount(),
			image.GetMetadata(),
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			DirectX::TEX_FILTER_SRGB,
			0,
			convertedImage
		);
		if ( SUCCEEDED(hr) ) {
			image = std::move(convertedImage);
		}
	}

	return std::move(image);
}

Microsoft::WRL::ComPtr<ID3D12Resource> TextureManager::CreateTextureResource(const DirectX::TexMetadata& metadata){
	// 1. metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc {};
	resourceDesc.Width = UINT(metadata.width);
	resourceDesc.Height = UINT(metadata.height);
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
	resourceDesc.Format = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Alignment = 0;
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
		initialState,
		nullptr, IID_PPV_ARGS(&resource));

	if ( FAILED(hr_) ) {
		// 基本ログ
		std::string log = std::format(
			"CreateTextureResource Failed. hr=0x{:08X}\nWidth:{} Height:{} MipLevels:{} Format:{}\n",
			static_cast<unsigned>(hr_),
			resourceDesc.Width, resourceDesc.Height, resourceDesc.MipLevels, static_cast<int>(resourceDesc.Format)
		);
		logManager.Log(log);

		// デバイス削除理由を問い合わせてログに残す
		if ( device_ ) {
			HRESULT removedReason = device_->GetDeviceRemovedReason();
			logManager.Log(std::format("GetDeviceRemovedReason: 0x{:08X}\n", static_cast<unsigned>(removedReason)));

#if defined(_DEBUG)
			// InfoQueue をダンプ（デバッグビルド限定）
			Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
			if ( SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue))) ) {
				UINT64 count = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
				for ( UINT64 i = 0; i < count; ++i ) {
					SIZE_T msgLen = 0;
					infoQueue->GetMessage(i, nullptr, &msgLen);
					std::vector<char> buf(msgLen);
					auto msg = reinterpret_cast<D3D12_MESSAGE*>(buf.data());
					infoQueue->GetMessage(i, msg, &msgLen);
					if ( msg->pDescription ) {
						logManager.Log(std::string("[D3D12] ") + msg->pDescription + "\n");
					}
				}
			}
#endif
		}

		// 失敗時は nullptr を返す（呼び出し元で判定して安全に処理）
		return nullptr;
	}

	return resource;
}


Microsoft::WRL::ComPtr<ID3D12Resource> TextureManager::UploadTextureData(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages, ID3D12GraphicsCommandList* commandList){
	// nullptr ガード
	if ( !texture ) {
		logManager.Log("UploadTextureData: destination texture is nullptr. Aborting upload.\n");
		return nullptr;
	}

	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device_.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, static_cast< UINT >( subresources.size() ));
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = resourceBuffer_->CreateBufferResource(intermediateSize);

	intermediateResources_.push_back(intermediateResource);

	/*D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	commandList->ResourceBarrier(1, &barrier);*/

	UpdateSubresources(commandList, texture.Get(), intermediateResource.Get(), 0, 0, static_cast< UINT >( subresources.size() ), subresources.data());

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	// StateBefore を COPY_DEST に合わせる (作成時の状態と一致させる)
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;

	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	commandList->ResourceBarrier(1, &barrier);

	return intermediateResource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> TextureManager::CreateDepthStencilTextureResource( int32_t width, int32_t height){
	
	// 生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc {};
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue {};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// Resourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&resource));

	if ( FAILED(hr) ) {
		logManager.Log(std::format("CreateDepthStencilTextureResource Failed. hr=0x{:08X} Width:{} Height:{} Format:{}\n",
			static_cast<unsigned>(hr), resourceDesc.Width, resourceDesc.Height, static_cast<int>(resourceDesc.Format)));
		if ( device_ ) {
			HRESULT removed = device_->GetDeviceRemovedReason();
			logManager.Log(std::format("GetDeviceRemovedReason: 0x{:08X}\n", static_cast<unsigned>(removed)));
#if defined(_DEBUG)
			Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
			if ( SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue))) ) {
				UINT64 cnt = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
				for ( UINT64 i = 0; i < cnt; ++i ) {
					SIZE_T len = 0;
					infoQueue->GetMessage(i, nullptr, &len);
					std::vector<char> buf(len);
					auto msg = reinterpret_cast<D3D12_MESSAGE*>(buf.data());
					infoQueue->GetMessage(i, msg, &len);
					if ( msg->pDescription ) logManager.Log(std::string("[D3D12] ") + msg->pDescription + "\n");
				}
			}
#endif
		}
		return nullptr;
	}

	return resource;
}
