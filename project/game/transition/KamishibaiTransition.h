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

	// 読み込み中のカードで次のシーンへ切り替える（カードが右から入る → 隠れている間に切り替え → 左へ引き抜く）。
	//   定義は resources/transition/card.json。カードの地は行き先で変わる（resources/kamishibai/card_bg_<シーン名>.png）。
	//   左右の辺は段ボールの切れ目（ギザギザ）で、カードの上では恐竜・卵・星などが動く（card_layout.json）。
	//   使い方は ChangeSceneWithCurtain と同じ
	static void ChangeSceneWithCard(const std::string& sceneName);

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
	bool  cardVisible_ = false;   // 読み込み中のカードを出しているか
	float cardOffset_ = 0.0f;     // カードの横へのずれ（画面幅=1。0＝画面にぴったり重なる）
	float cardAngle_ = 0.0f;      // カードの傾き(rad)
	// 切れ目で切って分かれる（部品 cardCut）
	float cutProgress_ = 0.0f;    // はさみが切り進んだ割合（0〜1。画面の左端→右端）
	float scissorsX_ = -1.0f;     // はさみの刃先の位置（画面幅=1。負なら出さない）
	float snipTime_ = 0.0f;       // はさみが切り始めてからの秒数（刃の開け閉め）
	float topLift_ = 0.0f, topTurn_ = 0.0f;       // 上の部分：上へのずれ（画面の高さ=1）と回り(rad)
	float bottomDrop_ = 0.0f, bottomTurn_ = 0.0f; // 下の部分：下へのずれと回り

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
	// 読み込み中のカードの地。下の折り目の線（切れ目）で上と下に分けてある（行き先で絵を差し替える）
	struct CardTextures { uint32_t top = 0, bottom = 0; };
	std::unique_ptr<Sprite> cardTop_;
	std::unique_ptr<Sprite> cardBottom_;
	std::unique_ptr<Sprite> cardSlit_;         // 切れ目が開いた線（はさみが進んだところまで見せる）
	std::unique_ptr<Sprite> scissors_;         // 切るはさみ（刃先が位置）
	std::unordered_map<std::string, CardTextures> cardTextures_; // 行き先のシーン名 → カードの地
	CardTextures defaultCardTextures_;         // 行き先の絵が無い時のカード（名前の帯なし）
	float cardMargin_ = 0.0f;                  // カードの左右の切れ目の幅（画像の px。画面に止まると画面の外）
	// 切れ目（画像の px。card_layout.json の "cut"）
	float cutY_ = 0.0f;
	float cardTopHeight_ = 0.0f, cardBottomTop_ = 0.0f, cardBottomHeight_ = 0.0f;
	float slitTop_ = 0.0f, slitHeight_ = 0.0f, slitWidth_ = 0.0f;
	float scissorsWidth_ = 0.0f, scissorsHeight_ = 0.0f;

	// カードの上で動く部品（恐竜・卵・星・雲・花・「…」の点など）
	enum class CardMotion { None, Walk, Rock, Twinkle, Drift, Sway, Hop, Dot };
	struct CardPiece {
		std::unique_ptr<Sprite> sprite;
		float x = 0.0f, y = 0.0f;           // 置き場所（カードの絵の座標。1920x1080 の中）
		float width = 0.0f, height = 0.0f;  // 画像の大きさ(px)
		float angle = 0.0f;                 // 置いた傾き(rad)
		float phase = 0.0f;                 // 動きのずらし
		int   index = 0;                    // 「…」の何個目か
		bool  bottom = false;               // 切れ目より下（切ると、下の部分と一緒に落ちる）
		CardMotion motion = CardMotion::None;
	};
	std::vector<CardPiece> cardPieces_;
	void LoadCardLayout();
	void DrawCard(float width, float height, float scaleX, float scaleY);
	std::unique_ptr<RenderTexture> snapshot_;  // 写し取った画面
	uint32_t snapshotWidth_ = 0;
	uint32_t snapshotHeight_ = 0;
	bool loaded_ = false;

	// --- エディタ ---
	char editDefinitionName_[64] = "title_open";
};
