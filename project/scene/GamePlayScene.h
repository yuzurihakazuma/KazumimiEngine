#pragma once
#define NOMINMAX
// --- エンジン側のファイル ---
#include "Engine/Scene/IScene.h"
#include "Engine/Math/Matrix4x4.h"
#include "Engine/graphics/TextureManager.h"

#include "game/card/HandManager.h"
#include "game/card/CardPickupManager.h"
#include "game/spawn/SpawnManager.h"

#include "Animation.h"
// --- 標準ライブラリ ---
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <random>

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
class Enemy;
class CardUseSystem;
class Boss;

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

	std::unique_ptr<Obj3d> testObj_ = nullptr;

	Vector3 playerPos_ = { 5.0f, 0.0f, 5.0f };
	Vector3 playerScale_ = { 1.0f, 1.0f, 1.0f };

	// マップエディタ
	std::unique_ptr<LevelEditor> levelEditor_;

	bool isEditorActive_ = true;

	//手札管理とプレイヤーコスト
	HandManager handManager_;

	CardPickupManager cardPickupManager_;

	std::unique_ptr<CardUseSystem> playerCardSystem_ = nullptr;
	std::vector<std::unique_ptr<CardUseSystem>> enemyCardSystems_;
	std::unique_ptr<CardUseSystem> bossCardSystem_ = nullptr;

	// スポーンマネージャー
	SpawnManager spawnManager_;

	// 敵関連
	std::vector<std::unique_ptr<Enemy>> enemies_;
	std::vector<std::unique_ptr<Obj3d>> enemyObjs_;
	std::vector<bool> enemyDeadHandled_;

	// ボス関連
	std::unique_ptr<Boss> boss_ = nullptr;
	std::unique_ptr<Obj3d> bossObj_ = nullptr;
	bool bossDeadHandled_ = false;

	int enemySpawnCount_ = 5;   // 出したい敵の数
	int enemySpawnMargin_ = 2;  // 壁から何マス離すか

	void ResetBattleDebug(); // デバッグ用バトルリセット

	//UI専用カメラ
	std::unique_ptr<Camera> uiCamera_ = nullptr;

	// カード交換モード用変数
	bool isCardSwapMode_ = false; // 交換モード中かどうか
	Card pendingCard_;            // 拾おうとしている（保留中の）カード

	//カード交換モードの処理
	void UpdateCardSwapMode(Input *input);

	//カード使用の処理
	void UpdateCardUse(Input *input);

	float dissolveThreshold_ = 0.0f; // ディゾルブエフェクトの進行度（0.0で通常、1.0で完全に消える）
	
	// 敵スポーンの処理
	void SpawnEnemiesRandom(int enemyCount, int margin);
	// カードスポーンの処理
	void SpawnCardsRandom(int cardCount, int margin);

	// カードスポーン関連
	int cardSpawnCount_ = 5;
	int cardSpawnMargin_ = 1;

	// ダンジョン再生成とプレイヤー再スポーンの処理
	void RegenerateDungeonAndRespawnPlayer(int roomCount);
	void RespawnPlayerInRoom();

	// ボス再配置
	void RespawnBossInRoom();

	// 敵とカードのクリア
	void ClearEnemiesAndCards();

	// 階層管理
	int currentFloor_ = 1;
	// 次の階層へ進む処理
	void AdvanceFloor();
	Animation testAnimation_;

	// 階段タイルの位置（タイル座標）
	std::pair<int, int> stairsTile_ = { -1, -1 };
	bool IsNearStairsTile(int x, int z) const;

};