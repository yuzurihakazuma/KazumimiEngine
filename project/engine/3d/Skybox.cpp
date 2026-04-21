#include "Skybox.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/graphics/PipelineManager.h"
#include "engine/graphics/SrvManager.h"

void Skybox::Initialize(const std::string& textureFilePath, ID3D12GraphicsCommandList* commandList){
	// 1. キューブマップテクスチャのロード
	textureData_ = TextureManager::GetInstance()->LoadTextureAndCreateSRVCube(textureFilePath, commandList);

	// 2. 球体モデルのポインタを取得 (ModelManagerで生成済みの前提)
	model_ = ModelManager::GetInstance()->FindModel("sphere");
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