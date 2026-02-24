#pragma once
// --- 標準・外部ライブラリ ---
#include <d3d12.h>

// 前方宣言
class DirectXCommon;

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