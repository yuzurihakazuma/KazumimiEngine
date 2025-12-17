#pragma once
#include <wrl.h>
#include <d3d12.h>
#include"DirectXCommon.h"
#include "LogManager.h"
#include"ShaderCompiler.h"


class Obj3dCommon{

	public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(DirectXCommon* dxCommon);
	// 共通の描画設定
	void PreDraw(ID3D12GraphicsCommandList* commandList);
	DirectXCommon* GetDxCommon() const{ return dxCommon_; }

private:

	DirectXCommon* dxCommon_ = nullptr; // 所有しない参照
};

