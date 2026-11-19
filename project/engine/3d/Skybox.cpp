#include "Skybox.h"
#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/camera/Camera.h"
#include "engine/graphics/PipelineManager.h"
#include "engine/graphics/SrvManager.h"

namespace KazumimiEngine {

namespace {
	constexpr const char* kSphereModel = "skySphere"; // 空を描く球（半径 1。大きさは関係ない）
	constexpr int kSphereSubdivision = 32;
}

void Skybox::Initialize(const std::string& textureFilePath){
	// 1. キューブマップテクスチャのロード
	textureData_ = TextureManager::GetInstance()->LoadCube(textureFilePath);

	// 2. 球体モデル（無ければ作る）
	ModelManager* modelManager = ModelManager::GetInstance();
	if ( !modelManager->FindModel(kSphereModel) ) { modelManager->CreateSphereModel(kSphereModel, kSphereSubdivision); }
	model_ = modelManager->FindModel(kSphereModel);
}

void Skybox::Draw(ID3D12GraphicsCommandList* commandList, Camera* camera){
	if ( !model_ || !camera ) return;

	// --- Skyboxの描画 ---
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Skybox);

	// [0]: カメラの定数バッファをセット (b0)
	commandList->SetGraphicsRootConstantBufferView(0, camera->GetConstantBufferAddress());

	// [1]: キューブマップテクスチャをセット (t0)
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(1, textureData_.srvIndex);

	// テクスチャセットを飛ばす DrawOnly を呼ぶ (マテリアルの上書きを防ぐ)
	model_->DrawOnly();
}

} // namespace KazumimiEngine
