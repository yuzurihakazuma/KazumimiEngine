#pragma once
#include "engine/base/Singleton.h"

struct ID3D12GraphicsCommandList;

namespace KazumimiEngine {

// 前方宣言
class DirectXCommon;

// =====================================================================
//  SpriteCommon：スプライトを描く前の、共通の描画設定。
//   パイプライン（シェーダー・ブレンド）と形の種類を、スプライトを描く前に 1 回だけ決める（PreDraw）。
//   全部の Sprite が同じ設定を使うので、1 つだけ作る（シングルトン）
// =====================================================================
class SpriteCommon : public Singleton<SpriteCommon> {
	friend class Singleton<SpriteCommon>; // 土台（Singleton）だけが作れる
public:
	/// <summary>
	/// 初期化
	/// </summary>
	/// 

	void Initialize(DirectXCommon* dxCommon);
	// 共通の描画設定
	void PreDraw(ID3D12GraphicsCommandList* commandList);

	// Getter（必要に応じて）
	DirectXCommon* GetDxCommon() const{ return dxCommon_; }


private:
	// コンストラクタを private にして外部からの生成を禁止
	SpriteCommon() = default;
	~SpriteCommon() = default;

private:

	DirectXCommon* dxCommon_ = nullptr; // 所有しない参照
	
};

} // namespace KazumimiEngine
