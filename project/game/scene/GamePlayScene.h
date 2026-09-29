#pragma once
// =====================================================================
//  GamePlayScene：ゲームプレイシーン。
//   各システム（レール・プレイヤー・敵・卵・ブロック・コイン・カメラ・演出）を持ち、
//   「どの順番で更新・描画するか」と「エディタの編集をいつ反映するか」だけを決める。
//   中身の処理はそれぞれのクラスへ分けてある：
//     見た目 … PlayerAvatar（プレイヤーのモデル）/ GameHud（画面の表示）/ PlayScreenEffects（画面効果）
//     エディタ … PlaySceneEditor（Game View の操作・パネル一式）/ EditorCameraUtil（カメラの置き直し）
//     判定 …… StageObstacles（卵の壁・地面）/ CombatSystem（踏みつけ・卵命中）
// =====================================================================
#include "engine/scene/IScene.h"
#include "engine/graphics/TextureManager.h"
#include "engine/utils/EditorManager.h" // EngineMode

#include "game/demo/DemoShowcase.h"
#include "game/rail/RailField.h"
#include "game/rail/RoadMesh.h"
#include "game/rail/DissolveRoad.h"
#include "game/camera/PlayCameraController.h"
#include "game/combat/HitFeel.h"
#include "game/combat/CombatSystem.h"
#include "game/player/SwallowAbility.h"
#include "game/player/AimThrowController.h"
#include "game/player/PlayerAvatar.h"
#include "game/stage/StageFlow.h"
#include "game/stage/CoinSystem.h"
#include "game/stage/BlockSystem.h"
#include "game/stage/StageObstacles.h"
#include "game/enemy/EnemyManager.h"
#include "game/egg/EggSystem.h"
#include "game/effect/PlayScreenEffects.h"
#include "game/ui/GameHud.h"
#include "game/editor/PlaySceneEditor.h"

#include <memory>
#include <string>
#include <unordered_map>

class Camera;
class DebugCamera;
class Player;
class EnemyEditor;

class GamePlayScene : public IScene {
public:
	GamePlayScene();
	~GamePlayScene();

	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;
	void DrawDebugUI() override;

private:
	// --- Initialize の段階 ---
	void LoadResources();    // BGM・SE・モデル・テクスチャの読み込み
	void SetupCameras();     // メインカメラ／デバッグカメラの生成・登録
	void SetupVisuals();     // 展示物・プレイヤーの見た目・HUD
	void SetupGameplay();    // プレイヤー・敵エディタ・レール・各種システム

	// --- エディタの編集をシーンへ反映（Update の最初）---
	void SyncFromEditors();
	void ApplyRailRemaps();        // レールの数・並びが変わった時の敵のレール番号の付け替え
	void SyncRailsLive();          // レール編集のライブ同期（ドラッグ中は10Hzの軽量同期）
	void SyncEnemyEdits();         // マップ読込の敵配置の復元・敵エディタの変更
	void SyncBlockEdits();         // ブロック配置のライブ同期
	void HandleCameraRequests();   // 「カメラをここへ」・Blender/カメラエディタからの画角
	// エディタの最新レールから railField_ を作り直し、敵・コイン・ブロックも配置し直す。
	//   simple=true はドラッグ中の軽量同期（道は簡易リボン・敵の再配置なし）
	void SyncRailsFromEditor(bool simple = false);
	void SpawnEnemies(); // 配置テンプレートを元に敵の実体を再構築する

	// --- Update の段階 ---
	void UpdateCameraAndPostEffect();              // カメラ更新＋シェイク＋ヒット点のポストエフェクト
	void HandleModeTransition(EngineMode current); // Edit↔Play 切替時のリセット
	void OnPlayStart();
	void OnEditStart();
	void UpdatePlayMode();                         // プレイ中のゲーム進行
	void UpdateSceneVisuals(EngineMode mode);      // モード問わず毎フレーム行う見た目の更新

	// --- Draw の段階 ---
	void DrawScene3D(ID3D12GraphicsCommandList* commandList);  // MRT へ3D一式
	void ComposeFrame(ID3D12GraphicsCommandList* commandList); // ポストエフェクト→Bloom→SDF→最終出力
	void DrawScreenUI(ID3D12GraphicsCommandList* commandList); // スプライト・HUD

	// --- DrawDebugUI の段階 ---
	void DrawInspectorUI(); // 共有の「インスペクター (詳細設定)」に足す項目
	void DrawHitShapes();   // 当たり判定の形をワイヤーで表示
	SceneEditContext MakeEditContext();

private:
	// カメラ
	std::unique_ptr<Camera>      camera_;
	std::unique_ptr<DebugCamera> debugCamera_;

	// 読み込んだテクスチャ（展示物にも名前で渡す）
	std::unordered_map<std::string, TextureData> textures_;
	std::string bgmFile_ = "resources/BGMDon.mp3";

	// エンジン機能の展示（ゲーム本編と無関係の見本一式。デモ表示OFFで丸ごと止まる）
	DemoShowcase demo_;

	EngineMode prevMode_ = EngineMode::Edit;

	// --- プレイヤー ---
	std::unique_ptr<Player> player_;
	PlayerAvatar      avatar_;    // 見た目（モデル・アニメ・手持ちの卵）
	SwallowAbility    swallow_;   // E=舌で捕まえる / 左Ctrl=産卵
	AimThrowController aimThrow_; // Q構え→狙う→離して投げる

	// --- ステージ ---
	RailField      railField_;    // 実行時レール（本体・緑線・動くレール）
	RoadMesh       roadMesh_;     // レール下の道メッシュ
	DissolveRoad   dissolveRoad_; // SDF溶け道（近づくと現れる道）
	StageFlow      stageFlow_;    // ゴール判定
	CoinSystem     coinSystem_;   // 収集物（コイン）
	BlockSystem    blockSystem_;  // 乗れる/ぶつかるブロック
	StageObstacles obstacles_;    // 卵・吐き出し弾の壁・地面判定（ブロック＋道）
	int lastBlockVersion_ = -1;   // ブロック編集のライブ同期用（レール全体は作り直さない）

	// --- 敵・卵・戦闘 ---
	EnemyManager                 enemyMgr_;
	std::unique_ptr<EnemyEditor> enemyEditor_; // 敵の配置テンプレート（配置の正本）
	EggSystem    eggSystem_;
	HitFeel      hitFeel_;  // ヒットストップ/シェイク/歪み+グロー
	CombatSystem combat_;   // 踏みつけ/卵命中の判定＋エフェクト

	// --- カメラ・画面 ---
	PlayCameraController camCtrl_;   // プレイ中カメラ（プレイヤー追従＋カメラ演出ゾーン）
	PlayScreenEffects    screenFx_;  // プレイ用の見た目・落下ミスのアイリス・起動時ウォームアップ
	GameHud              hud_;       // 卵の数・コインの数

	// --- エディタ ---
	PlaySceneEditor editorTools_;          // Game View の操作・配置ビュー・ミニマップ
	int   lastMapLoadVersion_ = -1;        // マップ読込を検知して敵配置を復元するため
	bool  railErasedPending_ = false;      // レール番号が変わった直後（敵の距離の張り直しを1回見送る）
	float railSyncTimer_ = 0.0f;           // ドラッグ中の道再生成を10Hzに間引くタイマー
	bool  railFullSyncPending_ = false;    // ドラッグ終了後に本同期を1回行うフラグ
	int   testPlayRail_ = -1;              // 「ここからテストプレイ」：次の Edit→Play の開始地点（-1=通常）
	float testPlayDist_ = 0.0f;
	bool  showDebugGrid_ = true;           // デバッグ描画のグリッド
	bool  showHitShapes_ = false;          // 当たり判定の形の表示
};
