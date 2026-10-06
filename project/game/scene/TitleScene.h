#pragma once
// =====================================================================
//  TitleScene：タイトルシーン（クラフトのジオラマ）。
//   タイトルそのものが小さなステージになっていて、本編と同じ仕組みで動く：
//     レール（RailField）と道（RoadMesh）… マップ resources/map/title.json のレールから自動で作る
//     恐竜の移動（Player）… 横レールは A/D、縦レールは W/S、角で乗り換え、Space でジャンプ
//
//   流れ：恐竜が奥のレールを走る → ロゴ看板を投げる → 角で縦レール→手前のレールへ乗り換えて中央へ
//         （ここまでは自動操縦。本編と同じ移動処理にキーの代わりの入力を与えている）
//         → A/D で的を選ぶと恐竜がその前まで歩く → SPACE でベロを当てて決定 → 卵を画面へ投げてゲーム開始
//
//   背景（地面・丘・木など動かない物）とレールはシーン専用のマップ title.json。
//     エディタで置いた物としてそのまま表示・編集・保存できる（ステージのマップとは別ファイル）
//   動く物はシーンが持つ：恐竜の見た目 TitleDino / ロゴ看板 TitleLogoBoard / メニュー TitleMenu /
//     紙ふぶき TitlePaperBits / 背景の動き TitleAmbience
//   カメラ・ポストエフェクト・デバッグ描画などの土台は BaseScene
// =====================================================================
#include "game/scene/BaseScene.h"
#include "game/player/Player.h"
#include "game/rail/RailField.h"
#include "game/rail/RoadMesh.h"
#include "game/title/TitleAmbience.h"
#include "game/title/TitleDino.h"
#include "game/title/TitleLogoBoard.h"
#include "game/title/TitleMenu.h"
#include "game/title/TitlePaperBits.h"
#include "engine/3d/obj/Obj3dCommon.h"
#include "engine/math/struct.h"

#include <memory>

class Obj3d;
class SDFText;

class TitleScene : public BaseScene {
public:
	TitleScene();
	~TitleScene() override;

	void Reload() override {}

private:
	// --- BaseScene の差し込み口 ---
	void OnLoadResources() override;
	void OnInitialize() override;
	void OnFinalize() override;
	void OnPreUpdate() override;   // エディタでレールを編集したら、レールと道を作り直す
	void OnUpdate() override;
	bool GetSdfViewerPosition(Vector3& outPos) const override;
	void OnDrawOpaque(ID3D12GraphicsCommandList* commandList) override;
	void OnDrawOverlay2D(ID3D12GraphicsCommandList* commandList) override; // 操作の案内（Game View にも映る）
	void OnDrawInspector() override;                                       // 調整項目

	// 進行の段階
	enum class Phase {
		Intro,         // 自動操縦：奥を走る → ロゴを投げる → 乗り換えて手前の中央へ
		Menu,          // 自分で歩いて的を選ぶ
		Aim,           // 決定：的のほうへ向き直る
		Tongue,        // ベロを的へ伸ばしている
		TurnToCamera,  // スタート決定：カメラのほうへ向き直る
		Throw,         // 卵を構えて投げる
		EggFlight,     // 卵が画面へ飛んでくる
		IrisOut,       // 画面を閉じる → ゲームプレイへ
		Done,
	};

	void SetupCamera();
	void ApplyLook();           // タイトルの見た目（画面効果・光）
	void RestoreLook();
	void SyncRails(bool simple = false); // マップのレールから、レール・道・スタート地点を作り直す
	void StartIntro();          // 登場から始める／やり直す
	void SkipIntro();           // 登場を飛ばしてメニューへ
	void EnterMenu();
	void ReleasePlayer();       // 決定の動きで止めていた移動と向きを元に戻す

	void UpdateIntro();
	PlayerInput::AutoPilot SteerAutoPilot(); // 次の角へ向かう入力を作る（登場の自動操縦）
	void UpdateMenu();
	void UpdateAim();
	void UpdateTongue();
	void UpdateEgg(float deltaTime);
	void UpdateIrisOut(float deltaTime);
	void UpdateTexts(float deltaTime);

	Phase phase_ = Phase::Intro;

	// --- 本編と同じ仕組み ---
	Player    player_;      // レール上の移動・乗り換え・ジャンプ
	RailField railField_;   // 実行時レール（マップから作る）
	RoadMesh  roadMesh_;    // レールの下の道
	float railSyncTimer_ = 0.0f;   // レールをドラッグ中の作り直しを間引くタイマー
	bool  railFullSyncPending_ = false;
	bool  editRails_ = false;      // エディタでレールを編集する（線とノードを出す）

	// --- タイトルの物 ---
	TitleDino      dino_;
	TitleLogoBoard logo_;
	TitleMenu      menu_;
	TitlePaperBits paperBits_;
	TitleAmbience  ambience_;   // 風でゆれる草花・流れる雲・紙の波
	bool logoThrown_ = false;

	// 登場の自動操縦
	int   waypointIndex_ = 0;      // 今目指している角
	int   pilotRail_ = -1;         // 前フレームに乗っていたレール（乗り換えた瞬間を知る）
	int   pilotReleaseFrames_ = 0; // キーを離しておくフレーム数（乗り換え直後は押し直しが要る）
	int   pilotSwitchWait_ = 0;    // 乗り換えキーを押し直すまでのフレーム数
	int   pilotHopFrames_ = 0;     // ジャンプを押しておくフレーム数（ロゴを投げる時の小ジャンプ）
	float introTime_ = 0.0f;

	// 決定の動き
	float aimYaw_ = 0.0f;          // 向き直る先
	float aimTime_ = 0.0f;
	float tongueDistance_ = 0.0f;  // 的までの距離
	bool  pendingDecide_ = false;  // 歩いている途中で決定が押された（着いたら決定する）

	// 決定時に投げる卵
	std::unique_ptr<Obj3d> egg_;
	bool    eggVisible_ = false;
	Vector3 eggFrom_ {};
	Vector3 eggTo_ {};
	float   eggTime_ = 0.0f;
	float   irisTime_ = 0.0f;

	// 文字（操作の案内と「じゅんびちゅう」）
	std::unique_ptr<SDFText> guideText_;
	std::unique_ptr<SDFText> noticeText_;
	float guideAlpha_ = 0.0f;
	float noticeTime_ = 0.0f;   // 残りの表示秒数
	float textTime_ = 0.0f;

	// 見た目の調整値（インスペクターから触れる）
	float tiltStrength_ = 6.0f;
	float tiltCenterY_ = 0.5f;
	float tiltHalfWidth_ = 0.27f;
	// タイトルの間だけ光の向きを変えるので、元の設定を覚えておいて終了時に戻す
	Obj3dCommon::DirectionalLightData savedLight_ {};
	float savedPointIntensity_ = 0.0f;
	float savedSpotIntensity_ = 0.0f;
	bool lightSaved_ = false;
};
