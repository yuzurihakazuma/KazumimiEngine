#include "SpriteCommon.h"
#include "PipelineManager.h"
using namespace logs;

void SpriteCommon::Initialize(DirectXCommon* dxCommon){
	assert(dxCommon);

	this->dxCommon_ = dxCommon;

	assert(this->dxCommon_->GetResourceFactory() != nullptr && "SpriteCommon: Received dxCommon has NO Factory!");

	
}

void SpriteCommon::PreDraw(ID3D12GraphicsCommandList* commandList){

	PipelineManager::GetInstance()->SetPipeline(
		commandList,
		PipelineType::Sprite
	);
}