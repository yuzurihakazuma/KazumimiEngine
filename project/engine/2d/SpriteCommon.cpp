#include "SpriteCommon.h"
#include <cassert>
#include <d3d12.h>          
#include "DirectXCommon.h"  
#include "PipelineManager.h"


// 初期化
void SpriteCommon::Initialize(DirectXCommon* dxCommon){
	assert(dxCommon);

	this->dxCommon_ = dxCommon;

	assert(this->dxCommon_->GetResourceFactory() != nullptr && "SpriteCommon: Received dxCommon has NO Factory!");

	
}

// 共通の描画設定
void SpriteCommon::PreDraw(ID3D12GraphicsCommandList* commandList){

	PipelineManager::GetInstance()->SetPipeline(
		commandList,
		PipelineType::Sprite
	);
}