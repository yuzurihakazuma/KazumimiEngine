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
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;
	void DrawDebugUI() override;

	TitleScene();
	~TitleScene();

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
};