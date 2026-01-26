#pragma once
#include "struct.h"
#include "Matrix4x4.h"
#include <d3d12.h>
#include <cstdint>
#include <wrl.h>
#include "Obj3dCommon.h"
#include "Model.h" // Modelクラスを使うため必須
#include "Camera.h"

// モデル3Dクラス
class Obj3d{

public:
	// 定数バッファ用データ構造体
	// ※座標（WVP/World）はオブジェクトごとに違うのでここに残す
	struct TransformationMatrix{
		Matrix4x4 WVP;
		Matrix4x4 World;
		Matrix4x4 WorldInverseTranspose;
	};

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(Model* model);

	/// <summary>
	/// 更新
	/// </summary>
	void Update();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw();


	// -------------------------------------------------
	// Getter / Setter
	// -------------------------------------------------

	const Vector3& GetScale() const{ return scale_; }
	const Vector3& GetRotation() const{ return rotation_; }
	const Vector3& GetTranslation() const{ return translate_; }

	void SetScale(const Vector3& scale){ scale_ = scale; }
	void SetRotation(const Vector3& rotation){ rotation_ = rotation; }
	void SetTranslation(const Vector3& translate){ translate_ = translate; }

	// モデルを差し替えるための関数
	void SetModel(Model* model){ model_ = model; }
	void SetCamera(const Camera* camera){ this->camera_ = camera; }


private:

	// -------------------------------------------------
	// メンバ変数
	// -------------------------------------------------

	// トランスフォーム（場所・回転・大きさ）
	Vector3 scale_;
	Vector3 rotation_;
	Vector3 translate_;

	// 行列計算用（計算を楽にするために保持）
	Transform transform { {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,0.0f} };
//	Transform cameraTransfrom { {1.0f,1.0f,1.0f}, {0.0f,0.0f,0.0f}, {0.0f,0.0f,-10.0f} };

	// 外部参照
	Obj3dCommon* obj3dCommon_ = nullptr; // 所有しない参照
	Model* model_ = nullptr;             // 描画するモデル（所有しない）

	// 座標変換行列リソース (WVP / World)
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_;
	TransformationMatrix* transformationMatrixData_ = nullptr;

	const Camera* camera_ = nullptr; // 所有しない参照


};