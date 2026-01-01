#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <unordered_map>
#include "PipelineType.h"
#include "DirectXCommon.h"
#include "ShaderCompiler.h"
#include "BlendMode.h"
using Microsoft::WRL::ComPtr;

// パイプラインマネージャー
class PipelineManager{
public:
	static PipelineManager* GetInstance(){
		static PipelineManager instance;
		return &instance;
	}

	void Initialize(DirectXCommon* dxCommon);

	void SetPipeline(
		ID3D12GraphicsCommandList* commandList,
		PipelineType type
	);

private:
	PipelineManager() = default;

	// Sprite用の初期化
	void CreateSpriteRootSignature();
	void CreateSpriteGraphicsPipeline();
	// OBJ用の初期化
	void CreateObject3DRootSignature();
	void CreateObject3DGraphicsPipeline();
	
	void CreateParticleRootSignature();
	void CreateParticleGraphicsPipeline();

	DirectXCommon* dxCommon_ = nullptr;

	ComPtr<ID3D12RootSignature> spriteRootSignature_;
	ComPtr<ID3D12PipelineState> spritePipelineState_;


	 
	ComPtr<ID3D12RootSignature> object3DRootSignature_;
	ComPtr<ID3D12PipelineState> object3DPipelineState_;

	// パーティクル用変数
	ComPtr<ID3D12RootSignature> particleRootSignature_;
	ComPtr<ID3D12PipelineState> particlePipelineState_;



	// 引数: 作成したルートシグネチャを格納する変数への参照
	// useInstancing: trueならパーティクル用、falseなら通常用として作ります
	void CreateRootSignatureCommon(Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSignature, bool useInstancing);

	// 汎用グラフィックスパイプライン生成
	// 引数で「違い」を受け取ることで、あらゆるパイプラインに対応します
	void CreateGraphicsPipelineCommon(
		const std::wstring& vsPath,   // VS(頂点シェーダー)のファイルパス
		const std::wstring& psPath,   // PS(ピクセルシェーダー)のファイルパス
		ID3D12RootSignature* rootSig, // 使用するルートシグネチャ
		BlendMode blendMode,          // ブレンドモード (Normal, Addなど)
		D3D12_CULL_MODE cullMode,     // カリングモード (None, Backなど)
		bool isDepthWrite,            // 深度を書き込むか (Spriteはfalse, 3Dはtrue)
		Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipelineState // 結果を入れる変数
	);


};

