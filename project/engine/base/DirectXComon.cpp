#include "DirectXComon.h"
#include <cassert>

using namespace logs;

void DirectXComon::Initialize(WindowProc* windowProc){
	// NULLチェック
	assert(windowProc);

	// メンバ変数にセット
	windowProc_ = windowProc;

	// デバイスの生成
	CreateFactory();
	// GPUアダプタの選択
	SelectAdapter();
	// D3D12デバイスの生成
	CreateDevice();
	// コマンド関連の初期化
	CreateCommand();
	// スワップチェーンの生成
	CreateSwapChain();
	// 深度バァファの生成
	CreateDepthBuffer();
	// 各種でスクリプタヒープの生成
	CreateDescriptorHeaps();
	// レンダーターゲットビューの初期化
	CreateRenderTargetViews();
	// 深度ステンシルビューの初期化
	CreateDepthStencilView();
	// フェンスの初期化
	CreateFence();
	// ビューポートの初期化
	InitializeViewport();
	// シザー矩形の初期化
	InitializeScissorRect();
	// DXCコンパイラの生成
	CreateDXCCompiler();
}





/// <summary>
/// DXGIファクトリーの生成
/// </summary>
void DirectXComon::CreateFactory(){
	// HRESULTはWindows系のエラーコードであり、
	// 関数が成功したかどうかをSUCCEDEDマクロで判定できる
	hr_ = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
	assert(SUCCEEDED(hr_));
}
/// <summary>
///  //GPUアダプタの選択
/// </summary>
void DirectXComon::SelectAdapter(){

	// いい順にアダプタを頼む
	for ( UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdaptr_)) !=
		DXGI_ERROR_NOT_FOUND; ++i ) {
		// アダプタ―の情報を習得する
		DXGI_ADAPTER_DESC3 adapterDesc {};
		hr_ = useAdaptr_->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr_));// 取得できないのは一大事
		// ソフトウェアアダプタでなければ採用！
		if ( !( adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE ) ) {
			// 採用したアダプタの情報をログに出力。wstringの方なので注意
			logManager_.Log(logManager_.ConvertString(std::format(L"Use Adapater:{}\n", adapterDesc.Description)));
			break;
		}
		useAdaptr_ = nullptr; // ソフトウェアアダプタの場合は見なかったことにする

	}
	// 適切なアダプタが見つからなかったので起動できない
	assert(useAdaptr_ != nullptr);


}
/// <summary>
/// D3D12デバイスの生成
/// </summary>
void DirectXComon::CreateDevice(){

	// 昨日レベルとログ出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0
	};
	const char* featureLevelStrings[] = { "12.2","12.1","12.0" };
	// 高い順に生成できるか試していく
	for ( size_t i = 0; i < _countof(featureLevels); ++i ) {
		// 採用したアダプターでデバイスを生成
		hr_ = D3D12CreateDevice(useAdaptr_.Get(), featureLevels[i], IID_PPV_ARGS(&device_));
		// 指定した機能レベルでデバイスが生成できたかを確認
		if ( SUCCEEDED(hr_) ) {
			// 生成できたのでログ出力を行ってループを抜ける
			logManager_.Log(std::format("FeatrueLevel : {}\n", featureLevelStrings[i]));
			break;
		}

	}
	// デバイスの生成がうまくいかなかったので起動できない
	assert(device_ != nullptr);
	logManager_.Log(logManager_.ConvertString(L"Complete create D3D12Device!!!\n"));// 初期化完了のログを出す
}
/// <summary>
/// コマンド関連の初期化
/// </summary>
void DirectXComon::CreateCommand(){
	assert(device_); // デバイスが生成済みであることを確認

	// コマンドキューの生成
	D3D12_COMMAND_QUEUE_DESC commandQuesDesc {};

	commandQuesDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // 標準的なコマンドキュー
	commandQuesDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL; // 標準的な優先度
	commandQuesDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // フラグ無し
	commandQuesDesc.NodeMask = 0; // マルチアダプタ用。通常は0

	// コマンドキューを生成する
	hr_ = device_->CreateCommandQueue(&commandQuesDesc, IID_PPV_ARGS(&commandQueue_));
	// コマンドキューの生成が上手くいかなかったので起動できない
	assert(SUCCEEDED(hr_));
	// コマンドアフロケータを生成
	hr_ = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
	// コマンドアロケータの生成が上手くいかなかったので起動出来ない
	assert(SUCCEEDED(hr_));
	// コマンドリストを生成する
	hr_ = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
	// コマンドリストの生成が上手くいかなかったので起動できない
	assert(SUCCEEDED(hr_));

	// ここ重要：初期はCloseしておく
	commandList_->Close();
}
/// <summary>
/// スワップチェーンの生成
/// </summary>
void DirectXComon::CreateSwapChain(){

	// スワップチェーンを生成する
	Microsoft::WRL::ComPtr<IDXGISwapChain1> tempSwapChain;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc {};
	swapChainDesc.Width = windowProc_->GetClientWidth();   // 画面の幅。ウィンドウのクライアント領域を同じものにしていく
	swapChainDesc.Height = windowProc_->GetClientHeight(); // 画面の高さ。ウィンドウのクライアント領域を同じようにしておく
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式
	swapChainDesc.SampleDesc.Count = 1; // マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして利用する
	swapChainDesc.BufferCount = 2; // ダブルバッファ
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニタにうつしたら、中身を破棄
	// コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	hr_ = dxgiFactory_->CreateSwapChainForHwnd(commandQueue_.Get(), windowProc_->GetHwnd(), &swapChainDesc, nullptr, nullptr, tempSwapChain.GetAddressOf());
	assert(SUCCEEDED(hr_));
	// IDXGISwapChain4を取得する
	hr_ = tempSwapChain.As(&swapChain_);
	assert(SUCCEEDED(hr_));

}
/// <summary>
/// 深度バァファの生成
/// </summary>
void DirectXComon::CreateDepthBuffer(){
	// 生成するResourceの設定
	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	desc.Width = static_cast< UINT >( windowProc_->GetClientWidth() ); // Textureの幅
	desc.Height = static_cast< UINT >( windowProc_->GetClientHeight() ); // Textureの幅
	desc.DepthOrArraySize = 1; // 奥行きor 配列Textureの配列数
	desc.MipLevels = 1; // mipmap
	desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencilとして利用可能なフォーマット
	desc.SampleDesc.Count = 1; // サンプリングカウント。1固定
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // 標準的なレイアウト
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // デプスステンシルとして使うのでフラグを立てる
	// 利用するHeapの設定。非常に特殊な運用・
	D3D12_HEAP_PROPERTIES heapProperties {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // 細かい設定を行う
	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue {};
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマット。Resourceと合わせる
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.DepthStencil.Stencil = 0;

	// 4. リソース作成
	hr_ = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue, IID_PPV_ARGS(&depthStencilResource_)
	);
	assert(SUCCEEDED(hr_));


}
/// <summary>
///  各種でスクリプタヒープの生成
/// </summary>
void DirectXComon::CreateDescriptorHeaps(){

	// RTV用のヒープでディスクリプタの数は2。RTVはshader内で触るものではないので,ShaderVisibleはfalse
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
	// SPV用のヒープでディスクリプタの数は128。SRVはShader内で触るものなので、ShaderVisibleはtrue
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
	// DSV用のヒープでディスクリプタの数は1。DSVはShader内で触るものではないので,ShaderVisibleはfalse
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHaap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
	// DescriptorSizeを取得しておく
	const uint32_t desriptorSizeSRV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t desriptorSizeRTV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t desriptorSizeDSV = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);



}
/// <summary>
/// レンダーターゲットビューの初期化
/// </summary>
void DirectXComon::CreateRenderTargetViews(){

	// DescriptorSizeを取得しておく
	rtvDescSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {}; // レンダーターゲットビューの設定
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2dテクスチャとして読み込む
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // リニア出力なら UNORM に変える
	rtvDesc.Texture2D.MipSlice = 0; // ミップマップの何番目を使うか
	rtvDesc.Texture2D.PlaneSlice = 0; // プレーン何番目を使うか

	// ヒープ先頭
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();

	// バックバッファごとにRTV作成
	for ( UINT i = 0; i < kBackBufferCount; ++i ) {
		hr_ = swapChain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i]));
		assert(SUCCEEDED(hr_));
		// RTVの生成
		device_->CreateRenderTargetView(backBuffers_[i].Get(), &rtvDesc, handle);

		// 次のディスクリプタへ
		handle.ptr += rtvDescSize_;
	}

}
/// <summary>
/// 深度ステンシルビューの初期化
/// </summary>
void DirectXComon::CreateDepthStencilView(){


	// DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc {};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // Format。基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; //2dTexture

	// ヒープの先頭ハンドル取得
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();

	// DSVHeapの設定にDSVをつくる
	device_->CreateDepthStencilView(depthBuffer_.Get(), &dsvDesc, dsvHandle);


}

/// <summary>
/// フェンスの初期化
/// </summary>
void DirectXComon::CreateFence(){

	// 初期化0でFenceを作る

	fenceValue_ = 0;
	hr_ = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
	assert(SUCCEEDED(hr_));

	// FenceのSignalを待つためのイベントを作成する
	HANDLE fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent_ != nullptr);

}
/// <summary>
/// ビューポートの初期化
/// </summary>
void DirectXComon::InitializeViewport(){

	// クライアントサイズ取得
	float width = static_cast< float >( windowProc_->GetClientWidth() );
	float height = static_cast< float >( windowProc_->GetClientHeight() );

	// クライアント領域のサイズと一緒にして画面全体に表示
	viewport_.Width = width;
	viewport_.Height = height;
	viewport_.TopLeftX = 0.0f;
	viewport_.TopLeftY = 0.0f;
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;

}
/// <summary>
/// シザー矩形の初期化
/// </summary>
void DirectXComon::InitializeScissorRect(){


	// クライアントサイズ取得
	float width = static_cast< LONG >( windowProc_->GetClientWidth() );
	float height = static_cast< LONG >( windowProc_->GetClientHeight() );

	// 基本的にビューボートと同じ矩形が構成されるようにする
	scissorRect_.left = 0;
	scissorRect_.right = width;
	scissorRect_.top = 0;
	scissorRect_.bottom = height;

}
/// <summary>
/// DXCコンパイラの生成
/// </summary>
void DirectXComon::CreateDXCCompiler(){


	hr_ = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	assert(SUCCEEDED(hr_));
	hr_ = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	assert(SUCCEEDED(hr_));
	hr_ = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	assert(SUCCEEDED(hr_));

}


/// <summary>
//  テクスチャリソースの生成
/// </summary>
Microsoft::WRL::ComPtr<ID3D12Resource> DirectXComon::CreateTextureResource(const DirectX::TexMetadata& metadata){

	// metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc {};
	resourceDesc.Width = UINT(metadata.width); // Textrueの幅
	resourceDesc.Height = UINT(metadata.height); // Textrueの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels); // mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize); // 奥行きor配列Textrueの配列数
	resourceDesc.Format = metadata.format; //TextrueのFormat 
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension); // Textrueの次元数。普段使っているのは2次元

	// 利用するHeapの設定。非常に特殊な運用・
	D3D12_HEAP_PROPERTIES heapProperties {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // 細かい設定を行う

	// Resourceの生成
	resource_ = nullptr;
	HRESULT hr_ = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr, IID_PPV_ARGS(&resource_));
	assert(SUCCEEDED(hr_));

	return resource_;

}
/// <summary>
// ディスクリプタヒープの生成
/// </summary>
/// <param name="device"></param>
/// <param name="heapType"></param>
/// <param name="numDescriptors"></param>
/// <param name="shaderVisible"></param>
/// <returns></returns>
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DirectXComon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible){
	// HRESULTはWindows系のエラーコードであり、
	// 関数が成功したかどうかをSUCCEDEDマクロで判定できる
	hr_ = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));

	// ディスクリプタヒープの生成
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc {};
	descriptorHeapDesc.Type = heapType; // 連打―ターゲットビュー用
	descriptorHeapDesc.NumDescriptors = numDescriptors; // ダブルバッファ用に2つ。多くても別に構わない。
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr_ = device_->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	// ディスクリプタヒープが作れなかったので起動できない
	assert(SUCCEEDED(hr_));
	return descriptorHeap;
}


#pragma region GetCPU

/// <summary>
///　CPUディスクリプタハンドルの取得
/// </summary>
/// <param name="descriptorHeap"></param>
/// <param name="descriptorSize"></param>
/// <param name="index"></param>
/// <returns></returns>
D3D12_CPU_DESCRIPTOR_HANDLE DirectXComon::GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index){

	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += ( descriptorSize * index );
	return handleCPU;
}
/// <summary>
///　CPUディスクリプタハンドルの取得
/// </summary>
D3D12_CPU_DESCRIPTOR_HANDLE DirectXComon::GetCPUDescriptorHandle(uint32_t descriptorSize, uint32_t index){
	return GetCPUDescriptorHandle(srvHeap_, descriptorSize, index);
}


#pragma endregion

#pragma region GetGPU

/// <summary>
///　GPUディスクリプタハンドルの取得
/// </summary>
/// <param name="descriptorHeap"></param>
/// <param name="descriptorSize"></param>
/// <param name="index"></param>
/// <returns></returns>
D3D12_GPU_DESCRIPTOR_HANDLE DirectXComon::GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index){

	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += ( descriptorSize * index );
	return handleGPU;
}
/// <summary>
///　GPUディスクリプタハンドルの取得
/// </summary>
D3D12_GPU_DESCRIPTOR_HANDLE DirectXComon::GetGPUDescriptorHandle(uint32_t descriptorSize, uint32_t index){

	return GetGPUDescriptorHandle(srvHeap_, descriptorSize, index);

}
#pragma endregion