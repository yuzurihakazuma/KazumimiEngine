#pragma once
// --- エンジン側のファイル ---
#include "Engine/Scene/IScene.h"
#include "Engine/Math/Matrix4x4.h"
#include "Engine/graphics/TextureManager.h"

// --- 標準ライブラリ ---
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>


// 前方宣言
class DebugCamera;
class Camera;
class Sprite;
class Obj3d;
class DirectXCommon;
class Input;
class RenderTexture; 
class PostEffect;
class LevelEditor;

	// ゲームプレイシーン
class GamePlayScene : public IScene {
public:
	// 初期化
	void Initialize() override;
	// 終了
	void Finalize() override;
	// 更新
	void Update() override;
	// 描画
	void Draw() override;

	GamePlayScene();

	~GamePlayScene();

private: // メンバ変数

	// カメラ
	std::unique_ptr<Camera> camera_ = nullptr;
	// デバッグカメラ
	std::unique_ptr<DebugCamera> debugCamera_ = nullptr;
	bool isDebugCameraActive_ = false;

	// デバッグカメラの前回の座標・回転（切り替えたときにカメラが飛ばないようにするため）
	Vector3 preDebugCameraPos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 preDebugCameraRot_ = { 0.0f, 0.0f, 0.0f };

	// 3Dオブジェクト
	std::vector<std::unique_ptr<Obj3d>> object3ds_;
	std::unique_ptr<Obj3d>fence_ = nullptr;
	std::unique_ptr<Obj3d> sphere_ = nullptr;
	std::unique_ptr<Obj3d> ground_ = nullptr;

	// スプライト
	std::vector<std::unique_ptr<Sprite>> sprites_;
	std::unique_ptr<Sprite> sprite_ = nullptr;

	Vector2 spritePos_ = { 100.0f, 100.0f };

	// テクスチャデータ
	std::unordered_map<std::string, TextureData> textures_;

	// デプスステンシル
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;

	// ゲーム用パラメータ
	Vector3 groundPos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 groundScale_ = { 1.0f, 1.0f, 1.0f };
	Vector3 spherePos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 sphereScale_ = { 1.0f, 1.0f, 1.0f };
	Vector3 fencePos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 fenceScale_ = { 1.0f, 1.0f, 1.0f };

	std::string bgmFile_ = "resources/BGMDon.mp3";

	// 描画先を切り替えるためのRenderTexture
	std::unique_ptr<PostEffect> postEffect_ = nullptr;

	// マップエディタ
	std::unique_ptr<LevelEditor> levelEditor_;

};