#pragma once
// --- 標準・外部ライブラリ ---
#include <wrl/client.h>
#include <d3d12.h>
#include <memory>
// --- エンジン側のファイル ---
#include "engine/math/Matrix4x4.h"
#include "engine/math/struct.h"

struct ID3D12Resource;

namespace KazumimiEngine {

// 前方宣言
class DirectXCommon;

// カメラクラス
class Camera{
public:
	// GPUに送るデータ構造体
	struct CameraForGPU{
		Matrix4x4 view;       // 追加：ビュー行列
		Matrix4x4 projection; // 追加：射影行列
		Vector3 worldPosition;
	};


	// コンストラクタ
	Camera(int windowWidth, int windowHeight, DirectXCommon* dxCommon);

	// 簡易生成：ウィンドウサイズと DirectXCommon を自動取得して生成する
	// 例: camera_ = Camera::Create();
	static std::unique_ptr<Camera> Create();

	// カメラ更新
	void Update();
	// デバッグ用UIの描画
	void DrawDebugUI();

	// setter/getter
	void SetRotation(const Vector3& rotation){ transform_.rotate = rotation; } // 回転をセット
	void SetTranslation(const Vector3& translation){ transform_.translate = translation; } // 位置をセット
	// position に置いて、target の方を向く（回転は X＝上下・Y＝左右。Z は 0）。
	//   演出のカメラ・追従カメラで同じ決め方をするので、ここにまとめる（前は使う所ごとに同じ計算を書いていた）
	void LookAt(const Vector3& position, const Vector3& target);
	void SetFovY(float fovY){ fovY_ = fovY; } // 視野角をセット
	float GetFovY() const{ return fovY_; }            // 視野角のgetter
	void SetAspectRatio(float aspectRatio){ aspectRatio_ = aspectRatio; } // アスペクト比をセット
	void SetNearClip(float nearClip){ nearClip_ = nearClip; } // ニアクリップ距離をセット
	void SetFarClip(float farClip){ farClip_ = farClip; } // ファークリップ距離をセット

	const Matrix4x4& GetWorldMatrix() const{ return worldMatrix_; } // ワールド行列のgetter
	const Matrix4x4& GetViewMatrix() const{ return viewMatrix_; } // ビュー行列のgetter
	const Matrix4x4& GetProjectionMatrix() const{ return projectionMatrix_; } // 射影行列のgetter
	const Matrix4x4& GetViewProjectionMatrix() const{ return viewProjectionMatrix_; } // ビュー射影行列のgetter
	const Transform& GetTransform() const{ return transform_; } // Transformのgetter
	const Vector3& GetRotation() const{ return transform_.rotate; } // 回転のgetter
	const Vector3& GetWorldPosition() const{ return transform_.translate; }
	ID3D12Resource* GetCameraResource() const{return cameraResource_.Get();}// カメラ用リソースのgetter

	D3D12_GPU_VIRTUAL_ADDRESS GetConstantBufferAddress() const{ return cameraResource_->GetGPUVirtualAddress(); }


private:
	
	DirectXCommon* dxCommon_ = nullptr; // DirectX共通クラスのポインタ
	
	// 変換行列
	Transform transform_; // カメラの位置・回転・スケール
	Matrix4x4 viewMatrix_; // ビュー行列
	Matrix4x4 worldMatrix_; // ワールド行列

	Matrix4x4 projectionMatrix_; // 射影行列
	Matrix4x4 viewProjectionMatrix_; // ビュー射影行列
	float fovY_ = 0.78f; // 視野角(垂直方向)
	float aspectRatio_ = 1.0f; // アスペクト比
	float nearClip_ = 0.1f; // ニアクリップ距離
	float farClip_ = 1000.0f; // ファークリップ距離

	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_; // カメラ用リソース
	CameraForGPU* cameraData_ = nullptr; // カメラデータのポインタ
	

};

} // namespace KazumimiEngine
