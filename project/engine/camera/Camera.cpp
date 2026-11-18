#include "Camera.h"
// --- 標準ライブラリ ---
#include <d3d12.h>
#include <cmath>

// --- エンジン側のファイル ---
#include "engine/base/WindowProc.h"
#include "engine/base/DirectXCommon.h"
#include "engine/graphics/ResourceFactory.h"
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace KazumimiEngine {

using namespace MatrixMath;

// 簡易生成：ウィンドウサイズと DirectXCommon を自動取得して生成
std::unique_ptr<Camera> Camera::Create(){
	WindowProc* window = WindowProc::GetInstance();
	return std::make_unique<Camera>(
		window->GetClientWidth(),
		window->GetClientHeight(),
		DirectXCommon::GetInstance());
}


void Camera::DrawDebugUI(){
#ifdef USE_IMGUI
	// "Inspector" などのウィンドウ名にすると、Unity風のインスペクターにまとめられます
	if ( ImGui::Begin("インスペクター (詳細設定)") ) {
		if ( ImGui::CollapsingHeader("カメラ設定 (Camera Settings)", ImGuiTreeNodeFlags_DefaultOpen) ) {
			// transform_ は Camera クラスのメンバ変数なので、直接編集できます！
			ImGui::DragFloat3("座標 (Position)", &transform_.translate.x, 0.1f);
			ImGui::DragFloat3("回転 (Rotation)", &transform_.rotate.x, 0.01f);

			// おまけ：視野角やクリップ距離も調整できるようにしておくと便利です
			ImGui::DragFloat("視野角 (FOV)", &fovY_, 0.01f);
			ImGui::DragFloat("ニアクリップ (Near Clip)", &nearClip_, 0.01f);
			ImGui::DragFloat("ファークリップ (Far Clip)", &farClip_, 1.0f);
		}
	}
	ImGui::End();
#endif
}


Camera::Camera(int windowWidth, int windowHeight, DirectXCommon* dxCommon)
	: dxCommon_(dxCommon)
	,transform_({ 1.0f,1.0f,1.0f, }, { 0.0f,0.0f,0.0f }, { 0.0f,0.0f,0.0f }) // 初期化
	, fovY_(0.78f) // 約45度
	, nearClip_(0.1f) // ニアクリップ距離
	, farClip_(100.0f) // ファークリップ距離
	, worldMatrix_(MakeAffine(transform_.scale, transform_.rotate, transform_.translate)) // ワールド行列の初期化
	, viewMatrix_(Inverse(worldMatrix_)) // ビュー行列の初期化
	, projectionMatrix_(PerspectiveFov(fovY_, aspectRatio_, nearClip_, farClip_)) // プロジェクション行列の初期化
	, viewProjectionMatrix_(Multiply(viewMatrix_, projectionMatrix_)) // ビュー射影行列の初期化
{

	// アスペクト比を計算
	aspectRatio_ = float(windowWidth) / float(windowHeight);

	cameraResource_ =
		dxCommon_->GetResourceFactory()->CreateBufferResource(sizeof(CameraForGPU));

	cameraResource_->Map(
		0, nullptr, reinterpret_cast< void** >( &cameraData_ )
	);


	// 初期化時点でも一度計算しておく
	Update();


}

// position に置いて、target の方を向く。target が position と同じ所なら、向きは変えない
void Camera::LookAt(const Vector3& position, const Vector3& target){
	transform_.translate = position;
	const Vector3 look { target.x - position.x, target.y - position.y, target.z - position.z };
	const float horizontal = std::sqrt(look.x * look.x + look.z * look.z);
	const float kTooClose = 1e-4f;
	if ( horizontal < kTooClose && std::abs(look.y) < kTooClose ) { return; }
	transform_.rotate = { std::atan2(-look.y, horizontal), std::atan2(look.x, look.z), 0.0f };
}

void Camera::Update(){

	// アスペクト比を計算
	aspectRatio_ = static_cast< float >( WindowProc::GetInstance()->GetClientWidth() ) /
		static_cast< float >( WindowProc::GetInstance()->GetClientHeight() );


	// 1. ワールド行列の更新 (カメラの場所・角度)
	worldMatrix_ = MakeAffine(transform_.scale, transform_.rotate, transform_.translate);

	// 2. ビュー行列の更新 (ワールドの逆行列)
	viewMatrix_ = Inverse(worldMatrix_);

	// 3. プロジェクション行列の更新 (画角など)
	projectionMatrix_ = PerspectiveFov(fovY_, aspectRatio_, nearClip_, farClip_);

	// 4. 合成行列の更新
	viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);

	cameraData_->view = viewMatrix_;             // ビュー行列を送る
	cameraData_->projection = projectionMatrix_; // 射影行列を送る

	// 5. GPU転送用データの更新
	cameraData_->worldPosition = transform_.translate;


}

} // namespace KazumimiEngine
