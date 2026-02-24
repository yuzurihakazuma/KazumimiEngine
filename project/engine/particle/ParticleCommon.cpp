#include "ParticleCommon.h"
// --- 標準ライブラリ ---
#include <cassert>

// --- エンジン側のファイル ---
#include "engine/base/DirectXCommon.h"
#include "engine/graphics/PipelineManager.h"


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