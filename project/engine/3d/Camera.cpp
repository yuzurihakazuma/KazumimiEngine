#include "Camera.h"
#include "WindowProc.h"
using namespace MatrixMath;

Camera::Camera(int windowWidth, int windowHeight)
	:transform({ 1.0f,1.0f,1.0f, }, { 0.0f,0.0f,0.0f }, { 0.0f,0.0f,0.0f }) // 初期化
	, fovY(0.78f) // 約45度
	, nearClip(0.1f) // ニアクリップ距離
	, farClip(100.0f) // ファークリップ距離
	, worldMatrix(MakeAffine(transform.scale, transform.rotate, transform.translate)) // ワールド行列の初期化
	, viewMatrix(Inverse(worldMatrix)) // ビュー行列の初期化
	, projectionMatrix(PerspectiveFov(fovY, aspectRatio, nearClip, farClip)) // プロジェクション行列の初期化
	, viewProjectionMatrix(Multiply(viewMatrix, projectionMatrix)) // ビュー射影行列の初期化
{

	// アスペクト比を計算
	aspectRatio = float(windowWidth) / float(windowHeight);

	// 初期化時点でも一度計算しておく
	Update();


}

void Camera::Update(){

	// 1. ワールド行列の更新 (カメラの場所・角度)
	worldMatrix = MakeAffine(transform.scale, transform.rotate, transform.translate);

	// 2. ビュー行列の更新 (ワールドの逆行列)
	viewMatrix = Inverse(worldMatrix);

	// 3. プロジェクション行列の更新 (画角など)
	projectionMatrix = PerspectiveFov(fovY, aspectRatio, nearClip, farClip);

	// 4. 合成行列の更新
	viewProjectionMatrix = Multiply(viewMatrix, projectionMatrix);

}
