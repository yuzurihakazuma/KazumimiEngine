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
	
	// 深度バァファの生成
	// 各種でスクリプタヒープの生成
	// レンダーターゲットビューの初期化
	// 深度ステンシルビューの初期化
	// フェンスの初期化
	// ビューポートの初期化
	// シザー矩形の初期化
	// DXCコンパイラの生成
	




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


