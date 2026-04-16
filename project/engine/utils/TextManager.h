#pragma once

// ---標準ライブラリ---
#include <string>
#include <unordered_map>
#include <memory> // スマートポインタ用

// ---外部ライブラリ---
#include "externals/imgui/imgui.h"

// ---DirectXTK12 用のヘッダー---
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <DescriptorHeap.h>
#include <GraphicsMemory.h>

// テキストデータの構造体
struct TextData{
	std::string text = "New Text";
	float x = 200.0f;
	float y = 200.0f;
	float scale = 1.0f;
	bool isCentered = false;
	float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };  // RGBAカラー (0.0f〜1.0f)
};

// テキストデータを管理するクラス
class TextManager{
public:
	// シングルトンインスタンスの取得
	static TextManager* GetInstance(){
		static TextManager instance;
		return &instance;
	}

	// 初期化
	void Initialize();

	void Finalize();

	// 描画処理
	void Draw();

	// デバッグUI描画
	void DrawDebugUI();

	// JSONファイルからテキストデータを読み込む
	void Save(const std::string& filePath = "resources/text/texts.json");
	void Load(const std::string& filePath = "resources/text/texts.json");

	// プログラム側からテキストを直接書き換える用
	void SetText(const std::string& key, const std::string& text);

	// 座標変更
	void SetPosition(const std::string& key, float x, float y);
	void SetCentered(const std::string& key, bool isCentered);

private:
	// 外部からのインスタンス生成を禁止
	TextManager() = default;
	~TextManager() = default;
	TextManager(const TextManager&) = delete;
	TextManager& operator=(const TextManager&) = delete;

private:
	// テキストデータをキーとセットで管理するマップ
	std::unordered_map<std::string, TextData> texts_;

	// デバッグUIの入力欄用の一時バッファ
	char inputKey_[64] = "";
	char inputText_[256] = "";

	// DirectXTK12 で文字を描くための必須パーツ
	std::unique_ptr<DirectX::GraphicsMemory> graphicsMemory_;
	std::unique_ptr<DirectX::DescriptorHeap> resourceDescriptors_;
	std::unique_ptr<DirectX::SpriteBatch> spriteBatch_;
	std::unique_ptr<DirectX::SpriteFont> spriteFont_;
};
