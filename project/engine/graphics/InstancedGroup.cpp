#include "InstancedGroup.h"
#include "Engine/3D/Model/ModelManager.h"
#include "Model.h"
#include "Engine/Base/DirectXCommon.h"
#include "Engine/Graphics/ResourceFactory.h"
#include "Engine/Graphics/PipelineManager.h"
#include "Engine/3D/Obj/Obj3dCommon.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Graphics/SrvManager.h"
#include <algorithm>
#include <cassert>
#include <cstring>

void InstancedGroup::Initialize(const std::string& modelName, uint32_t maxInstanceCount) {
	maxInstanceCount_ = maxInstanceCount;
	model_ = ModelManager::GetInstance()->FindModel(modelName);
	assert(model_);

	// 1万個分の巨大な配列バッファをGPUに作る
	auto dxCommon = DirectXCommon::GetInstance();
	instancingResource_ = dxCommon->GetResourceFactory()->CreateBufferResource(sizeof(Obj3d::TransformationMatrix) * maxInstanceCount_);
	instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));

	dissolveResource_ = dxCommon->GetResourceFactory()->CreateBufferResource(sizeof(Obj3d::DissolveData));
	dissolveResource_->Map(0, nullptr, reinterpret_cast<void**>(&dissolveData_));
	dissolveData_->threshold = 0.0f;

}

void InstancedGroup::Update(const std::vector<std::unique_ptr<Obj3d>>& objects) {
	currentInstanceCount_ = 0;

	// ブロックたちの計算結果を、GPU送信用配列にどんどん書き写す！
	for (const auto& obj : objects) {
		if (currentInstanceCount_ < maxInstanceCount_) {
			instancingData_[currentInstanceCount_] = obj->GetMatrixData();
			currentInstanceCount_++;
		}
		else {
			break; // 最大数を超えたらストップ
		}
	}
}

void InstancedGroup::Update(const std::vector<Obj3d::TransformationMatrix>& matrices) {
	currentInstanceCount_ = ( std::min )( ( uint32_t ) matrices.size(), maxInstanceCount_ );
	if ( currentInstanceCount_ > 0 ) {
		std::memcpy(instancingData_, matrices.data(), sizeof(Obj3d::TransformationMatrix) * currentInstanceCount_);
	}
}

void InstancedGroup::Update(const std::vector<Obj3d*>& objects) {
	currentInstanceCount_ = 0;
	for (Obj3d* obj : objects) {
		if (!obj) continue;
		if (currentInstanceCount_ >= maxInstanceCount_) break;
		instancingData_[currentInstanceCount_] = obj->GetMatrixData();
		currentInstanceCount_++;
	}
}

void InstancedGroup::Draw(const Camera* camera) {
	if (currentInstanceCount_ == 0 || !model_) return;

	auto commandList = DirectXCommon::GetInstance()->GetCommandList();

	// 1. パイプラインを「一括描画専用(InstancedObject3D)」に切り替え！
	// タイル貼りのモデル（UV が 0〜1 を超えて繰り返す）は、テクスチャを縦もリピートする版で描く（Obj3d と同じ判断）
	PipelineManager::GetInstance()->SetPipeline(commandList,
		model_->UsesTiledUV() ? PipelineType::InstancedObject3D_WrapUV : PipelineType::InstancedObject3D);

	// 2. 1万個分の配列を直接GPUにドーンと渡す！ (t0)
	commandList->SetGraphicsRootShaderResourceView(1, instancingResource_->GetGPUVirtualAddress());

	// 3. 全員共通のライトやカメラのデータを渡す
	commandList->SetGraphicsRootConstantBufferView(3, Obj3dCommon::GetInstance()->GetLightResource()->GetGPUVirtualAddress());
	if (camera) {
		commandList->SetGraphicsRootConstantBufferView(4, camera->GetCameraResource()->GetGPUVirtualAddress());
	}
	commandList->SetGraphicsRootConstantBufferView(5, Obj3dCommon::GetInstance()->GetPointLightResource()->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(6, Obj3dCommon::GetInstance()->GetSpotLightResource()->GetGPUVirtualAddress());

	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(7, noiseTextureIndex_);
	commandList->SetGraphicsRootConstantBufferView(8, dissolveResource_->GetGPUVirtualAddress());

	// [9] 環境マップ (t3)。ルートシグネチャが宣言している以上、未束縛のまま Draw すると
	// デバッグレイヤーの ERROR（DESCRIPTOR_TABLE_NOT_SET）で停止する。Obj3d::Draw と同じ扱いに揃える
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(9,
		Obj3dCommon::GetInstance()->GetEnvironmentTextureSrvIndex());

	// 4. いざ、一括描画！！ (集めた個数だけ一気に描く！)
	model_->Draw(currentInstanceCount_);
}