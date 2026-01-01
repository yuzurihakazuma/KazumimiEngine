#pragma once
#include <d3d12.h>

// ブレンドモードの種類
enum class BlendMode{
	kNone,      // ブレンドなし
	kNormal,    // 通常αブレンド
	kAdd,       // 加算合成
	kSubtract,  // 減算合成
	kMultiply,  // 乗算合成
	kScreen,    // スクリーン合成
};

// ブレンド設定を取得
D3D12_BLEND_DESC GetBlendDesc(BlendMode blendMode);