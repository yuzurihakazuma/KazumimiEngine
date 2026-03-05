#pragma once

// --- 標準ライブラリ・外部ライブラリ ---
#include <memory>
#include <d3d12.h>
#include <string>
// --- エンジン側のファイル ---
#include "engine/graphics/RenderTexture.h"

class DirectXCommon;
class SrvManager;

enum class PostEffectType {
	None,           // エフェクトなし（そのまま表示）
	Grayscale,      // グレースケール
	Sepia,          // セピア
	Vignetting,     // ビネット（周辺減光）
	BoxFilter,      // ボックスフィルター（ぼかし）
	BoxFilter5x5,   // ボックスフィルター 5x5（強めのぼかし）
	GaussianFilter, // ガウシアンフィルター（綺麗なぼかし）
	Count           // 種類の数
};


class PostEffect {
public:

	static PostEffect* GetInstance() {
		static PostEffect instance;
		return &instance;
	}

	~PostEffect() = default;

	// 初期化
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, uint32_t width, uint32_t height);

	void Finalize();

	// お絵かき開始（画用紙への切り替え）
	void PreDrawScene(ID3D12GraphicsCommandList* commandList, DirectXCommon* dxCommon);

	// お絵かき終了（メイン画面への切り替え）
	void PostDrawScene(ID3D12GraphicsCommandList* commandList, DirectXCommon* dxCommon);

	// ポストエフェクトの描画（巨大な三角形を描く）
	void Draw(ID3D12GraphicsCommandList* commandList);

	// デバッグ用UIの描画
	void DrawDebugUI();

	
	void Save(const std::string& filePath = "resources/data/postEffect.json"); // 現在のエフェクト設定をJSONファイルに保存
	void Load(const std::string& filePath = "resources/data/postEffect.json"); // JSONファイルからエフェクト設定を読み込む

private:

	PostEffect() = default;
	PostEffect(const PostEffect&) = delete;
	PostEffect& operator=(const PostEffect&) = delete;

private:
	// 内部でRenderTexture（画用紙）を所有する
	std::unique_ptr<RenderTexture> renderTexture_ = nullptr;

	// 現在のエフェクトの種類
	PostEffectType currentType_ = PostEffectType::None;

	bool isActive_ = false;

};