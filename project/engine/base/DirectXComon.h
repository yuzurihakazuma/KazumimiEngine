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
	/// <summary>初期化</summary>
	void Initialize(WindowProc* windowProc);

	void PreDraw();  // 描画前処理
	void PostDraw(); // 描画後処理

	/// <summary>テクスチャ用のリソース作成
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);

	/// <summary>ディスクリプタヒープ作成
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
		D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);

	/// <summary>指定番号のCPUディスクリプタハンドルを取得
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
		const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
		uint32_t descriptorSize,
		uint32_t index);
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t descriptorSize, uint32_t index);

	/// <summary>指定番号のGPUディスクリプタハンドルを取得
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
		const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
		uint32_t descriptorSize,
		uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t descriptorSize, uint32_t index);

	// SRVヒープ
	ID3D12DescriptorHeap* GetSrvHeap() const{ return srvDescriptorHeap_.Get(); }

	/// Deviceのゲッター
	ID3D12Device* GetDevice() const{ return device_.Get(); }

	/// <summary>コマンドリストのゲッター
	ID3D12GraphicsCommandList* GetCommandList() const{ return commandList_.Get(); }

private:

	// -------------------- 初期化・生成系 --------------------

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

	// -------------------- DXGI・デバイス関連 --------------------
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;    // DXGIファクトリー
	Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter_;        // 使用するアダプタ
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdaptr_ = nullptr; // 選択されたアダプタ
	Microsoft::WRL::ComPtr<ID3D12Device> device_;          // D3D12デバイス

	// -------------------- コマンド関連 --------------------
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;        // コマンドキュー
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_; // コマンドアロケータ
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;   // コマンドリスト

	// -------------------- スワップチェーン・バックバッファ --------------------
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;                // スワップチェーン
	static constexpr UINT kBackBufferCount = 2;                        // バックバッファの数
	Microsoft::WRL::ComPtr<ID3D12Resource> backBuffers_[kBackBufferCount]; // バックバッファ
	UINT rtvDescSize_ = 0;                                             // RTVのディスクリプタサイズ

	// -------------------- ディスクリプタヒープ --------------------
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;   // RTV用のヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;   // SRV用のヒープ（ディスクリプタの数は128）
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHaap_;   // DSV用のヒープ（ディスクリプタの数は1）

	uint32_t desriptorSizeSRV_; // SRVのディスクリプタサイズ
	uint32_t desriptorSizeRTV_; // RTVのディスクリプタサイズ
	uint32_t desriptorSizeDSV_; // DSVのディスクリプタサイズ

	// -------------------- 深度・ステンシル --------------------
	Microsoft::WRL::ComPtr<ID3D12Resource> depthBuffer_;           // 深度バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;  // ステンシルリソース

	// -------------------- その他リソース --------------------
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_; // 汎用リソース（用途によって変更可）

	// -------------------- システム関連 --------------------
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_; // フェンス
	HANDLE fenceEvent_ = nullptr;           // フェンス用イベントハンドル
	uint64_t fenceValue_ = 0;               // カレントのフェンス値
	D3D12_VIEWPORT viewport_ {};            // ビューポート
	D3D12_RECT scissorRect_ {};             // シザー矩形

	// -------------------- DXCコンパイラ関連 --------------------
	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;                 // DXCユーティリティ
	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;          // DXCコンパイラ
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;  // インクルードハンドラ

	// -------------------- その他 --------------------
	HRESULT hr_;                      // HRESULT保存用
	LogManager logManager_;          // ログマネージャー
	WindowProc* windowProc_ = nullptr; // ウィンドウプロシージャ


};

