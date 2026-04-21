#pragma once

#include "Engine/Scene/IScene.h"
#include "Engine/Math/Matrix4x4.h"
#include "Engine/Graphics/TextureManager.h"

// 標準ライブラリ
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

// 前方宣言
class DebugCamera;
class Camera;
class Obj3d;
class RenderTexture;
class PostEffect;
class MapManager;

// タイトルシーン
class TitleScene : public IScene {
public:

	// タイトル背景のカード雨用構造体
	struct TitleRainCard {
		std::unique_ptr<Obj3d> obj;
		Vector3 position;
		Vector3 rotation;
		Vector3 rotationSpeed;
		float fallSpeed = 0.08f;
	};
	// タイトル背景のカード雨
	std::vector<std::string> titleRainCardModelNames_ = {
		"cardR",
		"cardF",
		"cardFire",
		"cardPotion",
		"cardSpeedUp",
		"CardShield",
		"CardIce",
		"CardFang",
		"CardDecoy",
		"CardAtkDown",
		"CardClaw",
		"CardScanner"
	};


	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;
	void DrawDebugUI() override;

	TitleScene();
	~TitleScene();

	void InitializeTitleCardRain();
	void UpdateTitleCardRain();
	void ResetTitleRainCard(TitleRainCard& card, bool randomY);

private:
	// カメラ
	std::unique_ptr<Camera> camera_ = nullptr;

	// デバッグカメラ
	std::unique_ptr<DebugCamera> debugCamera_ = nullptr;

	// 3Dオブジェクト
	std::vector<std::unique_ptr<Obj3d>> object3ds_;

	// テクスチャデータ
	std::unordered_map<std::string, TextureData> textures_;

	// デプスステンシル
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;

	std::string bgmFile_ = "resources/BGMDon.mp3";

	// マップエディタ
	std::unique_ptr<MapManager> mapManager_ = nullptr;

	bool isEditorActive_ = true;

	// タイトル背景用
	Vector3 titleBgCameraPos_ = { 9.0f, 35.0f, 9.0f };
	Vector3 titleBgCameraRot_ = { 1.57f, 0.0f, 0.0f };
	Vector3 titleBgPlayerPos_ = { 0.0f, 0.0f, 0.0f };

	// タイトル背景のカード雨
	std::vector<TitleRainCard> titleRainCards_;

	// カード雨のパラメータ
	int titleRainCardCount_ = 24;

	float titleRainSpawnX_ = 36.0f;
	float titleRainResetX_ = 62.0f;

	float titleRainY_ = 7.0f;

	float titleRainSpawnMinZ_ = 38.0f;
	float titleRainSpawnMaxZ_ = 60.0f;

	float titleRainScale_ = 1.0f;


};