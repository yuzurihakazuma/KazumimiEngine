#pragma once
#include "Matrix4x4.h"
#include "struct.h"


// カメラクラス
class Camera{
public:
	// コンストラクタ
	Camera(int windowWidth, int windowHeight);

	// カメラ更新
	void Update(); 


	// setter/getter
	void SetRotation(const Vector3& rotation){ transform.rotate = rotation; } // 回転をセット
	void SetTranslation(const Vector3& translation){ transform.translate = translation; } // 位置をセット
	void SetFovY(float fovY){ this->fovY = fovY; } // 視野角をセット
	void SetAspectRatio(float aspectRatio){ this->aspectRatio = aspectRatio; } // アスペクト比をセット
	void SetNearClip(float nearClip){ this->nearClip = nearClip; } // ニアクリップ距離をセット
	void SetFarClip(float farClip){ this->farClip = farClip; } // ファークリップ距離をセット

	const Matrix4x4& GetWorldMatrix() const{ return worldMatrix; } // ワールド行列のgetter
	const Matrix4x4& GetViewMatrix() const{ return viewMatrix; } // ビュー行列のgetter
	const Matrix4x4& GetProjectionMatrix() const{ return projectionMatrix; } // 射影行列のgetter
	const Matrix4x4& GetViewProjectionMatrix() const{ return viewProjectionMatrix; } // ビュー射影行列のgetter
	const Transform& GetTransform() const{ return transform; } // Transformのgetter
	const Vector3& GetRotation() const{ return transform.rotate; } // 回転のgetter


private:
	// 変換行列
	Transform transform; // カメラの位置・回転・スケール
	Matrix4x4 viewMatrix; // ビュー行列
	Matrix4x4 worldMatrix; // ワールド行列

	Matrix4x4 projectionMatrix; // 射影行列
	Matrix4x4 viewProjectionMatrix; // ビュー射影行列
	float fovY = 0.78f; // 視野角(垂直方向)
	float aspectRatio = 1.0f; // アスペクト比
	float nearClip = 0.1f; // ニアクリップ距離
	float farClip = 1000.0f; // ファークリップ距離

};

