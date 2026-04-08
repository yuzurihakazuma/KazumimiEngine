#pragma once
#include <string>
#include "engine/math/struct.h"

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