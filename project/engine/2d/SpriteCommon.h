#pragma once
#include <wrl.h>
#include <d3d12.h>
#include"DirectXCommon.h"
#include "LogManager.h"
#include"ShaderCompiler.h"

using namespace logs;



class SpriteCommon{
public:
	/// <summary>
	/// 初期化
	/// </summary>
	/// 
	static SpriteCommon* GetInstance(){
		static SpriteCommon instance;
		return &instance;
	}

	void Initialize(DirectXCommon* dxCommon);
	// 共通の描画設定
	void PreDraw(ID3D12GraphicsCommandList* commandList);

	// Getter（必要に応じて）
	DirectXCommon* GetDxCommon() const{ return dxCommon_; }


private:
	// コンストラクタを private にして外部からの生成を禁止
	SpriteCommon() = default;
	~SpriteCommon() = default;
	SpriteCommon(const SpriteCommon&) = delete;
	SpriteCommon& operator=(const SpriteCommon&) = delete;

private:

	DirectXCommon* dxCommon_ = nullptr; // 所有しない参照
	
};

