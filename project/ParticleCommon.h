#pragma once
#include "DirectXCommon.h"

class ParticleCommon{
public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon);

	// 共通の描画設定
	void PreDraw(ID3D12GraphicsCommandList* commandList);

	DirectXCommon* GetDxCommon() const{ return dxCommon_; }

private:
	DirectXCommon* dxCommon_ = nullptr;
};