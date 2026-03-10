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
	Outline,        // アウトライン
	RandomNoise,    // ランダムノイズ
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
	
	// 更新
	void Update();

	// 終了処理
	void Finalize();

	// お絵かき開始（画用紙への切り替え）
	void PreDrawScene(ID3D12GraphicsCommandList* commandList, DirectXCommon* dxCommon);

	// お絵かき終了（メイン画面への切り替え）
	void PostDrawScene(ID3D12GraphicsCommandList* commandList, DirectXCommon* dxCommon);

	// ポストエフェクトの描画（巨大な三角形を描く）
	void Draw(ID3D12GraphicsCommandList* commandList, DirectXCommon* dxCommon);
	// デバッグ用UIの描画
	void DrawDebugUI();

	// エフェクトの有効化・無効化
	void Save(const std::string& filePath = "resources/data/postEffect.json"); // 現在のエフェクト設定をJSONファイルに保存
	void Load(const std::string& filePath = "resources/data/postEffect.json"); // JSONファイルからエフェクト設定を読み込む

	uint32_t GetSrvIndex() const;


	// 外から時間を進めるための関数
	void AddTime(float addValue){
		time_ += addValue;
		if ( timeData_ ) {
			*timeData_ = time_;
		}
	}

	// ビューポートの位置とサイズを設定する関数
	void SetViewArea(float x,float y, float width, float height) {		
		viewX_ = x;
		viewY_ = y;
		viewWidth_ = width;
		viewHeight_ = height;
	}

private:

	PostEffect() = default;
	PostEffect(const PostEffect&) = delete;
	PostEffect& operator=(const PostEffect&) = delete;

private:
	// 内部でRenderTexture（画用紙）を所有する
	std::unique_ptr<RenderTexture> renderTexture_ = nullptr;
	std::unique_ptr<RenderTexture> resultTexture_ = nullptr;


	// 現在のエフェクトの種類
	PostEffectType currentType_ = PostEffectType::None;

	bool isActive_ = true;
	// 時間経過で変化するエフェクトのための時間管理
	Microsoft::WRL::ComPtr<ID3D12Resource> timeResource_;
	float* timeData_ = nullptr; // GPUに送る用の時間データ
	float time_ = 0.0f;         // C++側でカウントアップする時間
	// 時間の進むスピード（エフェクトの変化の速さ）
	float timeSpeed_ = 0.05f;
	// ビューポートの位置とサイズ
	float viewX_ = 0.0f;
	float viewY_ = 0.0f;
	float viewWidth_ = 1280.0f;
	float viewHeight_ = 720.0f;

};