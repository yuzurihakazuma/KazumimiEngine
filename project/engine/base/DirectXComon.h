#pragma once
#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <d3d12.h>
#include<cassert>
#include "ShaderCompiler.h"
#include "LogManager.h"
#include "WindowProc.h"
#include "externals/DirectXTex/DirectXTex.h"
using namespace logs;


class DirectXComon{
public:
	/// <summary>
	// 初期化
	/// </summary>
	void Initialize(WindowProc* windowProc);
	/// <summary>
	// 終了処理
	/// </summary>
	void Finalize();
/// <summary>
/// 
/// </summary>
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);
	/// <summary>
	/// </summary>
	/// <param name="device"></param>
	/// <param name="heapType"></param>
	/// <param name="numDescriptors"></param>
	/// <param name="shaderVisible"></param>
	/// <returns></returns>
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType,UINT numDescriptors, bool shaderVisible);
	/// <summary>
	/// 指定番号のCPUディスクリプタハンドルを取得
	/// </summary>
	/// <param name="descriptorHeap"></param>
	/// <param name="descriptorSize"></param>
	/// <param name="index"></param>
	/// <returns></returns>
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t descriptorSize, uint32_t index);
	/// <summary>
	///　指定番号のGPUディスクリプタハンドルを取得
	/// </summary>
	/// <param name="descriptorHeap"></param>
	/// <param name="descriptorSize"></param>
	/// <param name="index"></param>
	/// <returns></returns>
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t descriptorSize, uint32_t index);

private:

	void CreateFactory(); // DXGIファクトリーの生成

	void SelectAdapter(); //GPUアダプタの選択

	void CreateDevice(); // D3D12デバイスの生成

	void CreateCommand(); // コマンド関連の初期化

	void CreateSwapChain(); // スワップチェーンの生成

	void CreateDepthBuffer(); // 深度バァファの生成

	void CreateDescriptorHeaps(); // 各種でスクリプタヒープの生成

	void CreateRenderTargetViews(); // レンダーターゲットビューの初期化

	void CreateDepthStencilView(); // 深度ステンシルビューの初期化

	void CreateFence(); // フェンスの初期化

	void InitializeViewport(); // ビューポートの初期化

	void InitializeScissorRect(); // シザー矩形の初期化

	void CreateDXCCompiler(); // DXCコンパイラの生成

	/// <summary>
	///  メンバ変数
	/// </summary>
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_; // DXGIファクトリー
	Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter_; // 使用するアダプタ
	Microsoft::WRL::ComPtr<ID3D12Device> device_; // D3D12デバイス

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_; // コマンドキュー
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_; // コマンドアロケータ
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_; // コマンドリスト

	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_; // スワップチェーン
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_; // レンダーターゲットビュー用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_; // 深度ステンシルビュー用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_; // シェーダーリソースビュー用デスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer_; // 深度バッファ
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_; // フェンス

	HANDLE fenceEvent_ = nullptr; // フェンス用イベントハンドル
	D3D12_VIEWPORT viewport_ {}; // ビューポート
	D3D12_RECT scissorRect_ {}; // シザー矩形

	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_; // DXCユーティリティ
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_; // DXCコンパイラ
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;; // インクルードハンドラ


	

	HRESULT hr_; // HRESULT保存用

	LogManager logManager_; // ログマネージャー

	WindowProc* windowProc_ = nullptr; // ウィンドウプロシージャ


	// 使用するアダプタ用の変数。最初にnullptrを入れておく
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdaptr_ = nullptr;
	// Resourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_ = nullptr;

	// Resourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_ = nullptr;


	static constexpr UINT kBackBufferCount = 2; // バックバッファの数
	Microsoft::WRL::ComPtr<ID3D12Resource>   backBuffers_[kBackBufferCount]; // バックバッファ
	UINT rtvDescSize_ = 0; // RTVのディスクリプタサイズ

	// フェンス本体
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_ = nullptr;

	// カレントのフェンス値
	uint64_t fenceValue_ = 0;

	// フェンス待ち用のイベントハンドル
	HANDLE fenceEvent_ = nullptr;

};

