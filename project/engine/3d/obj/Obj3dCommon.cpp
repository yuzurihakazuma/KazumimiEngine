#include "Obj3dCommon.h"

// --- 標準ライブラリ ---
#include <cassert>
#include "externals/imgui/imgui.h"
#include <numbers>

// --- エンジン側のファイル ---
#include "engine/graphics/PipelineManager.h"
#include "engine/math/Matrix4x4.h"
#include "engine/base/DirectXCommon.h"
#include "engine/math/VectorMath.h"
#include "engine/graphics/ResourceFactory.h"

using namespace VectorMath;
using namespace MatrixMath;

void Obj3dCommon::DrawDebugUI() {
#ifdef USE_IMGUI
	if (ImGui::Begin("Inspector")) {
		if (ImGui::CollapsingHeader("Lighting Settings", ImGuiTreeNodeFlags_DefaultOpen)) {

			// --- 平行光源 (Directional Light) ---
			if (ImGui::TreeNode("Directional Light")) {
				// クラス内のメンバ変数(構造体)を直接編集できるのでシンプルに書けます
				ImGui::DragFloat3("Direction", &directionalLightData_->direction.x, 0.01f);
				ImGui::ColorEdit3("Color", &directionalLightData_->color.x);
				ImGui::DragFloat("Intensity", &directionalLightData_->intensity, 0.01f, 0.0f, 10.0f);
				directionalLightData_->direction = Normalize(directionalLightData_->direction);
				ImGui::TreePop();
			}

			// --- 点光源 (Point Light) ---
			if (ImGui::TreeNode("Point Light")) {
				ImGui::DragFloat3("Position", &pointLightData_->position.x, 0.01f);
				ImGui::ColorEdit3("Color", &pointLightData_->color.x);
				ImGui::DragFloat("Intensity", &pointLightData_->intensity, 0.01f, 0.0f, 10.0f);
				ImGui::DragFloat("Radius", &pointLightData_->radius, 0.1f, 0.1f, 100.0f);
				ImGui::DragFloat("Decay", &pointLightData_->decay, 0.1f, 0.1f, 10.0f);
				ImGui::TreePop();
			}

			// --- スポットライト (Spot Light) ---
			if (ImGui::TreeNode("Spot Light")) {
				ImGui::DragFloat3("Position", &spotLightData_->position.x, 0.01f);
				ImGui::DragFloat3("Direction", &spotLightData_->direction.x, 0.01f);
				spotLightData_->direction = Normalize(spotLightData_->direction); // 常に正規化
				ImGui::ColorEdit3("Color", &spotLightData_->color.x);
				ImGui::DragFloat("Intensity", &spotLightData_->intensity, 0.01f, 0.0f, 10.0f);
				ImGui::DragFloat("Distance", &spotLightData_->distance, 0.1f, 0.1f, 100.0f);
				ImGui::DragFloat("Decay", &spotLightData_->decay, 0.1f, 0.1f, 10.0f);
				ImGui::DragFloat("cosAngle", &spotLightData_->cosAngle, 0.01f, -1.0f, 1.0f);
				ImGui::DragFloat("FalloffStart", &spotLightData_->cosFalloffStart, 0.01f, -1.0f, 1.0f);
				ImGui::TreePop();
			}
		}
	}
	ImGui::End();
#endif
}


// 初期化
void Obj3dCommon::Initialize(DirectXCommon* dxCommon){
	// NULLチェック
	assert(dxCommon);
	// メンバ変数にセット
	this->dxCommon_ = dxCommon;
	// FactoryのNULLチェック
	assert(this->dxCommon_->GetResourceFactory() != nullptr && "SpriteCommon: Received dxCommon has NO Factory!");

	directionalLightResource_ =
		dxCommon_->GetResourceFactory()->CreateBufferResource(sizeof(DirectionalLight));

	directionalLightResource_->Map(
		0, nullptr, reinterpret_cast< void** >( &directionalLightData_ )
	);
	// ライトの初期化
	directionalLightData_->color = { 1,1,1,1 };
	directionalLightData_->direction = Normalize({ 0,-1,0 });
	directionalLightData_->intensity = 1.0f;

	// 点光源のバッファ作成
	pointLightResource_ = dxCommon_->GetResourceFactory()->CreateBufferResource(sizeof(PointLight));
	pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData_));

	// 初期値（スライド資料の通り、位置を(0,2,0)にしておく）
	pointLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	pointLightData_->position = { 0.0f, 2.0f, 0.0f };
	pointLightData_->intensity = 1.0f;
	pointLightData_->radius = 10.0f;
	pointLightData_->decay = 1.0f;

	// スポットライトのバッファ作成
	spotLightResource_ = dxCommon_->GetResourceFactory()->CreateBufferResource(sizeof(SpotLight));
	spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData_));

	spotLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	spotLightData_->position = { 2.0f, 1.25f, 0.0f };
	spotLightData_->distance = 7.0f;
	spotLightData_->direction = Normalize({ -1.0f, -1.0f, 0.0f });
	spotLightData_->intensity = 4.0f;
	spotLightData_->decay = 2.0f;
	spotLightData_->cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f); // 約0.5 (60度)
	spotLightData_->cosFalloffStart = 1.0f; // 1.0なら最初から減衰が始まる
}

// 共通の描画設定
void Obj3dCommon::PreDraw(ID3D12GraphicsCommandList* commandList){
	// パイプラインセット
	PipelineManager::GetInstance()->SetPipeline(
		commandList,
		PipelineType::Object3D 
	);
}


void Obj3dCommon::Finalize() {
	directionalLightResource_.Reset();

	 pointLightResource_.Reset();
	 spotLightResource_.Reset();
}