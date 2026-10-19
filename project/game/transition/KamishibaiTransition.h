#pragma once
// =====================================================================
//  KamishibaiTransition：紙芝居の遷移（紙芝居遷移システム 企画書）。
//   シーンマネージャに差し込み（SceneManager::SetTransition）、シーンが変わっても残る舞台として描く。
//
//   舞台：段ボールの枠＋観音開きの扉（画面に重ねる2D。枠をくぐる＝枠を大きくして画面の外へ逃がす）
//   幕：赤い色画用紙の蛇腹の幕（左右から閉じて、開くと次のシーン）。紙芝居の枠・扉の代わりに使える
//   部品：シーンの退場（シーンの段ボールがバタバタ倒れる。IScene::BeginExitEffect）／絵の引き抜き（今の画面を1枚の絵として写し取り、横へ吹き飛ばす）／扉の開閉／
//         「はじまり、はじまり」の帯／扉に投げる卵／枠をくぐる／効果音／次のシーンの読み込み／おわり
//   部品の並びと長さは resources/transition/<名前>.json（インスペクターの「紙芝居遷移」で調整・保存できる）
//
//   遷移中はゲーム側のキー操作を止める（Input::SetLocked）。早送り・飛ばしは無い
// =====================================================================
#include "engine/scene/ISceneTransition.h"
#include "game/transition/StageIntroCard.h"
#include "game/transition/TransitionTimeline.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Sprite;
class RenderTexture;

class KamishibaiTransition : public ISceneTransition {
public:
	static KamishibaiTransition* GetInstance();

	// 画像・音を先に読む（実行中の読み込みは D3D12 のデバッグレイヤーが嫌うので、起動時に）
	void Load();

	// 遷移を始める（resources/transition/<definitionName>.json）。
	//   今の画面を写し取って絵にするので、呼んだフレームの画面がそのまま「吹き飛ぶ絵」になる。
	//   定義が読めなければ false（呼び出し側は別の切り替え方をする）
	//   toScene を渡すと、定義の "to" の代わりにそのシーンへ切り替える
	bool Play(const std::string& definitionName, const std::string& toScene = "");

	// 赤い幕で次のシーンへ切り替える（幕を閉じる → 閉じている間に切り替え → 幕を開ける）。
	//   定義は resources/transition/curtain.json。sceneName は SceneFactory の名前（"TITLE" / "STAGESELECT" / "GAMEPLAY"）。
	//   遷移の最中に呼んだら何もしない（二重に切り替えない）。幕の定義が読めない時は、そのまま切り替える
	static void ChangeSceneWithCurtain(const std::string& sceneName);

	// 読み込み中のカードで次のシーンへ切り替える（台紙が入り、星・帯・キャラ・行き先の名前が1つずつ貼られる →
	//   隠れている間に切り替え → はさみで切って分かれる）。カードはステージ開始カードと同じ（StageIntroCard）。
	//   定義は resources/transition/card.json、帯に出す名前は resources/stage_intro/card_<行き先のシーン>.json。
	//   使い方は ChangeSceneWithCurtain と同じ
	static void ChangeSceneWithCard(const std::string& sceneName);

	// ステージ開始カードで次のシーン（ステージ）へ切り替える（ステージ開始演出 企画書）。
	//   台紙が出る → 星・名前の帯・キャラが貼られる → 裏で読み込み → はさみで切って分かれる →
	//   プレイヤーが振り向いてアクション → カメラが後ろへ戻って操作開始（シーンへは OnTransitionCue で合図を送る）。
	//   定義は resources/transition/stage_intro.json、ステージごとの中身は resources/stage_intro/<stage>.json
	static void ChangeSceneWithStageIntro(const std::string& sceneName, const std::string& stage);

	// --- ISceneTransition ---
	void Update(float deltaTime) override;
	bool IsActive() const override{ return active_; }
	void DrawInto(ID3D12GraphicsCommandList* commandList, RenderTexture* target) override;
	void DrawOverlay(ID3D12GraphicsCommandList* commandList) override;
	void DrawDebugUI() override;
	void Finalize() override;

private:
	KamishibaiTransition() = default;

	// definitionName の遷移で sceneName へ切り替える（遷移中は何もしない。定義が無ければそのまま切り替える）
	static void ChangeSceneWith(const std::string& definitionName, const std::string& sceneName);
	bool LoadDefinition(const std::string& definitionName);
	bool SaveDefinition() const;
	void ApplyParts(float deltaTime); // 部品の進み具合から、絵・扉・枠などの見た目を決める
	void Finish();
	void Capture(ID3D12GraphicsCommandList* commandList, RenderTexture* target);
	void DrawLayers(ID3D12GraphicsCommandList* commandList, float width, float height);

	// --- 定義 ---
	std::string definitionName_;
	std::string toScene_ = "STAGESELECT"; // 定義の行き先（SceneFactory の名前。保存される）
	std::string playTo_;                  // 今の再生で切り替える先（Play で行き先を渡した時はそれ）
	TransitionTimeline timeline_;

	// --- 再生の状態 ---
	bool  active_ = false;
	bool  sceneRequested_ = false;
	bool  loadedFired_ = false;
	bool  captureRequested_ = false;
	bool  snapshotValid_ = false;
	bool  stageVisible_ = false;  // 紙芝居の枠と扉を出しているか（シーンの退場が終わって、絵を引き抜く時から）
	bool  exitFired_ = false;

	// --- 見た目（ApplyParts が決め、DrawInto が描く）---
	bool  pictureVisible_ = false;
	float pictureOffset_ = 0.0f;  // 絵の横へのずれ（画面幅=1）
	float pictureLift_ = 0.0f;    // 絵の上へのずれ（画面高さ=1）
	float pictureAngle_ = 0.0f;   // 絵の傾き(rad)
	float doorOpen_ = 0.0f;       // 扉の開き（0=閉じている / 1=開き切った。行き過ぎて1を超えることもある）
	float frameZoom_ = 1.0f;      // 枠の大きさ（くぐる時に大きくなる）
	float frameAlpha_ = 1.0f;
	float bannerScale_ = 0.0f;
	float bannerAlpha_ = 0.0f;
	bool  eggVisible_ = false;
	float eggX_ = 0.0f, eggY_ = 0.0f; // 画面の割合（0〜1）
	float eggAngle_ = 0.0f;
	float eggAlpha_ = 1.0f;
	float eggFallTime_ = -1.0f;   // 扉に当たってから落ちていく時間（-1=まだ当たっていない）
	float curtainClose_ = 0.0f;   // 赤い幕の閉じ具合（0=開いている / 1=真ん中で合わさっている）
	float valanceDrop_ = 0.0f;    // 上の幕板の下がり具合（0=画面の上の外 / 1=下りている）

	// --- 描く物 ---
	std::unique_ptr<Sprite> frame_;
	std::unique_ptr<Sprite> doorLeft_;
	std::unique_ptr<Sprite> doorRight_;
	std::unique_ptr<Sprite> banner_;
	std::unique_ptr<Sprite> egg_;
	std::unique_ptr<Sprite> picture_;          // 写し取った画面を貼る板
	std::unique_ptr<Sprite> curtainLeft_;      // 赤い幕（右は同じ絵を左右反転）
	std::unique_ptr<Sprite> curtainRight_;
	std::unique_ptr<Sprite> valance_;          // 幕の上の段ボールの幕板と三角旗
	StageIntroCard stageIntro_;               // カード（読み込み中・ステージ開始で共通。部品 introElement / introCut / letterbox など）
	std::string    stageIntroContent_;        // 次に再生するカードの中身（ステージ名。空なら行き先のシーンから決める）
	std::unique_ptr<RenderTexture> snapshot_;  // 写し取った画面
	uint32_t snapshotWidth_ = 0;
	uint32_t snapshotHeight_ = 0;
	bool loaded_ = false;

	// --- エディタ ---
	char editDefinitionName_[64] = "title_open";
};
