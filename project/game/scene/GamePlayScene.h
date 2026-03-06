#pragma once
// --- エンジン側のファイル ---
#include "Engine/Scene/IScene.h"
#include "Engine/Math/Matrix4x4.h"
#include "Engine/graphics/TextureManager.h"

#include "game/card/HandManager.h"
#include "game/card/CardPickupManager.h"

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
class Player;
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

	// デバッグ用UIの描画
	void DrawDebugUI() override;


	GamePlayScene();

	~GamePlayScene();

private: // メンバ変数

	// カメラ
	std::unique_ptr<Camera> camera_ = nullptr;
	// デバッグカメラ
	std::unique_ptr<DebugCamera> debugCamera_ = nullptr;

	// 3Dオブジェクト
	std::vector<std::unique_ptr<Obj3d>> object3ds_;

	// スプライト
	std::vector<std::unique_ptr<Sprite>> sprites_;
	std::unique_ptr<Sprite> sprite_ = nullptr;

	Vector2 spritePos_ = { 100.0f, 100.0f };

	// テクスチャデータ
	std::unordered_map<std::string, TextureData> textures_;

	// デプスステンシル
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;


	std::string bgmFile_ = "resources/BGMDon.mp3";

	// 描画先を切り替えるためのRenderTexture
	std::unique_ptr<PostEffect> postEffect_ = nullptr;

	std::unique_ptr<Player> player_ = nullptr;

	std::unique_ptr<Obj3d> playerObj_ = nullptr;

	Vector3 playerPos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 playerScale_ = { 1.0f, 1.0f, 1.0f };

	// マップエディタ
	std::unique_ptr<LevelEditor> levelEditor_;

	bool isEditorActive_ = true;

	//手札管理とプレイヤーコスト
	HandManager handManager_;
	int dummyPlayerCost_ = 3;


	CardPickupManager cardPickupManager_;

};