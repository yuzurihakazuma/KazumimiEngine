#include "ParticleCommon.h"
#include "PipelineManager.h"
#include <cassert>

void ParticleCommon::Initialize(DirectXCommon* dxCommon){
	assert(dxCommon);
	dxCommon_ = dxCommon;
}

void ParticleCommon::PreDraw(ID3D12GraphicsCommandList* commandList){
	assert(commandList);

	// パーティクル用のパイプラインをセットする
	PipelineManager::GetInstance()->SetPipeline(
		commandList,
		PipelineType::Particle // ★これを新しく作る必要があります
	);
}