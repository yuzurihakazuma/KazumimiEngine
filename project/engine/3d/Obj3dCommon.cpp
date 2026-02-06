#include "Obj3dCommon.h"

// 標準ライブラリ
#include <cassert>

// エンジン独自ライブラリ
#include "PipelineManager.h"
#include "Matrix4x4.h"
#include "DirectXCommon.h"


using namespace MatrixMath;

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

	directionalLightData_->color = { 1,1,1,1 };
	directionalLightData_->direction = Normalize({ 0,-1,0 });
	directionalLightData_->intensity = 1.0f;
}

// 共通の描画設定
void Obj3dCommon::PreDraw(ID3D12GraphicsCommandList* commandList){
	// パイプラインセット
	PipelineManager::GetInstance()->SetPipeline(
		commandList,
		PipelineType::Object3D 
	);
}