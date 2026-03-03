#include "PostEffect.h"
#include "engine/graphics/PipelineManager.h"
#include "engine/graphics/SrvManager.h"

void PostEffect::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height) {
	// RenderTextureの生成と初期化
	renderTexture_ = std::make_unique<RenderTexture>();
	renderTexture_->Initialize(dxCommon, srvManager, width, height);
}

void PostEffect::PreDrawScene(ID3D12GraphicsCommandList* commandList, DirectXCommon* dxCommon) {
	renderTexture_->PreDrawScene(commandList, dxCommon);
}

void PostEffect::PostDrawScene(ID3D12GraphicsCommandList* commandList, DirectXCommon* dxCommon) {
	renderTexture_->PostDrawScene(commandList, dxCommon);
}

void PostEffect::Draw(ID3D12GraphicsCommandList* commandList) {
	// 1. パイプライン設定
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::PostEffect);

	// 2. トポロジー設定（三角形で描画）
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 3. SRV（描き込んだ画像）をシェーダーに渡す
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, renderTexture_->GetSrvIndex());

	// 4. 描画コマンド（画面全体を覆う三角形）
	commandList->DrawInstanced(3, 1, 0, 0);
}