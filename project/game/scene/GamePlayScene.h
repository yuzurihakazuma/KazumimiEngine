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
#include "InstancedGroup.h"
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
class Minimap;
class EnemyManager;
class BossManager;
class MapManager;

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
	std::unique_ptr<MapManager> mapManager_;

	bool isEditorActive_ = true;

	//手札管理とプレイヤーコスト
	HandManager handManager_;

	CardPickupManager cardPickupManager_;

	// カード
	std::unique_ptr<CardUseSystem> playerCardSystem_ = nullptr;

	// スポーンマネージャー
	SpawnManager spawnManager_;

	
	std::unique_ptr<EnemyManager> enemyManager_;

	std::unique_ptr<BossManager> bossManager_ = nullptr;

	int enemySpawnCount_ = 5;   // 出したい敵の数
	int enemySpawnMargin_ = 2;  // 壁から何マス離すか

	void ResetBattleDebug(); // デバッグ用バトルリセット

	Vector2 WorldToScreen(const Vector3& worldPos) const; // ワールド座標をスクリーン座標に変換する関数

	//UI専用カメラ
	std::unique_ptr<Camera> uiCamera_ = nullptr;

	// カード交換モード用変数
	bool isCardSwapMode_ = false; // 交換モード中かどうか
	Card pendingCard_;            // 拾おうとしている（保留中の）カード
	struct CardPickup *pendingPickup_ = nullptr; // 拾おうとしているフィールドのアイテム
	int swapSelectionIndex_ = 0;                 // 捨てる手札を選ぶカーソル位置

	//カード交換モードの処理
	void UpdateCardSwapMode(Input *input);

	//カード使用の処理
	void UpdateCardUse(Input *input);

	// ポーズ画面の更新
	void UpdatePause(Input* input);

	// ポーズ画面の描画
	void DrawPauseUI();

	float dissolveThreshold_ = 0.0f; // ディゾルブエフェクトの進行度（0.0で通常、1.0で完全に消える）
	
	
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

	// カードの説明文の後ろに敷く背景画像テクスチャ
	uint32_t descBgTexture_ = 0;
	std::unique_ptr<Sprite> descBgSprite_ = nullptr;

	// ステータス無限モード(デバッグ用)
	bool isInfiniteMode_ = false;
	// --- フェード演出用の状態 ---
	enum class TransitionState {
		None,       // 通常時
		FadeOut,    // 暗くなっている途中
		FadeIn      // 明るくなっている途中
	};

	// private メンバ変数に追加
	TransitionState transitionState_ = TransitionState::None;
	float fadeAlpha_ = 0.0f;           // 0.0f(透明) ～ 1.0f(真っ黒)
	const float kFadeSpeed = 0.02f;   // フェードの速さ（調整してください）

	// 画面全体を覆う黒スプライト
	std::unique_ptr<Sprite> fadeSprite_ = nullptr; // または std::unique_ptr<Sprite> fadeSprite_;

	// プレイヤーステータスの背景
	std::unique_ptr<Sprite> playerStatusBgSprite_ = nullptr;

	// コスト不足メッセージ表示用
	int costLackMessageTimer_ = 0;


	// ブロックの一括描画用グループ
	std::unique_ptr<InstancedGroup> blockGroup_ = nullptr;
	std::vector<std::unique_ptr<Obj3d>> blocks_;

	std::unique_ptr<Minimap> minimap_;

	bool isCardReady_ = false;       // 現在カードを構えているか
	Card readiedCard_{};             // 構えているカードの情報
	int cardReadyTimer_ = 0;         // 構えていられる残り時間

	// ポーズ画面関連
	bool isPaused_ = false;                  // ポーズ中かどうか
	int pauseSelection_ = 0;                 // 0: Resume  1: Title

	std::unique_ptr<Sprite> pauseBgSprite_ = nullptr; // ポーズ中の半透明背景
};