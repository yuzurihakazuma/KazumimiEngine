#pragma once

#include <memory>
#include <d3d12.h>
#include "engine/graphics/RenderTexture.h"

class DirectXCommon;
class SrvManager;

class PostEffect {
public:
	// 初期化
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height);

	// お絵かき開始（画用紙への切り替え）
	void PreDrawScene(ID3D12GraphicsCommandList* commandList, DirectXCommon* dxCommon);

	// お絵かき終了（メイン画面への切り替え）
	void PostDrawScene(ID3D12GraphicsCommandList* commandList, DirectXCommon* dxCommon);

	// ポストエフェクトの描画（巨大な三角形を描く）
	void Draw(ID3D12GraphicsCommandList* commandList);

private:
	// 内部でRenderTexture（画用紙）を所有する
	std::unique_ptr<RenderTexture> renderTexture_ = nullptr;
};