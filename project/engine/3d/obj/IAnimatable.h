#pragma once
#include <string>
#include "engine/math/struct.h"

// =====================================================================
//  IAnimatable：位置・回転・大きさを外から動かせる物の、共通の形（インターフェース）。
//   中身の無い関数（純粋仮想関数）だけを持ち、Obj3d と SkinnedObj3d がこれを継いで実装する。
//   アニメーションの編集（AnimationEditor）は、相手がどちらでも、この形を通して同じように動かす（ポリモーフィズム）
// =====================================================================
class IAnimatable {
public:

	// デストラクタ
	virtual ~IAnimatable() = default; 

	
	
	virtual const Vector3& GetTranslation() const = 0;  // 位置の取得

	virtual const Vector3& GetRotation() const = 0; // 回転の取得

	virtual const Vector3& GetScale() const = 0;    // スケールの取得

	virtual void SetTranslation(const Vector3& translation) = 0; // 位置の設定

	virtual void SetRotation(const Vector3& rotation) = 0; // 回転の設定

	virtual void SetScale(const Vector3& scale) = 0;    // スケールの設定

	virtual void Update() = 0; // アニメーションの更新（例: キーフレームの補間など）
	virtual void Draw() = 0; // 描画（必要に応じてオーバーライド）

	// UI表示用の名前
	virtual const std::string& GetName() const = 0;
	virtual void SetName(const std::string& name) = 0; // 名前の設定

};