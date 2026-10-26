#pragma once
// =====================================================================
//  GameOverSequence：ゲームオーバー演出の進行（ゲームオーバー演出 企画・仕様書 Rev 1.1）。
//   P0 止まる        … ゲームの時間を止めて「ポン」。恐竜はやられポーズ
//   P1 種明かし      … カメラが手前へ引き、ステージが段ボール箱の中のジオラマだったと分かる（箱はステージの範囲から自動で作る）
//   P4 写し取る      … その瞬間の画面を1回だけレンダーターゲットへ写し、3D を隠して「1枚の紙」に貼り替える（見た目は途切れない）
//   P3/P5 くしゃくしゃ… 画面そのものが、真ん中へぎゅっと縮みながら折れ目だらけになる（グシャッ）
//   捨てる           … くしゃくしゃの紙が奥へ飛んでいき、ゲームオーバーの画面になる
//   P7 ゲームオーバーの画面 … 「GAME OVER」「やりなおす」「ステージをえらぶ」（GameOverScreen）
//   P8 組み上げ      … 「やりなおす」：画面がしまわれ、くしゃくしゃの紙が手前へ戻ってきながら広がると、
//                      チェックポイント（スタート）の画面になって、そのまま再開
//                      「ステージをえらぶ」：紙芝居の遷移でステージ選択へ
//   ・ゲームの時間は止め、演出は自分の時間（止まらない時間）で進める。カメラは写し取った所から動かさない
//   ・P4 で見えていない間に、シーンはチェックポイントの状態へ戻しておく（死んだ瞬間を逆再生しない）
//   ・再開の絵は、1フレームだけ再開のカメラで描いて写し取る（そのフレームは前の画面を出し直すので見えない）
//   ・2回目以降は短縮版（P1 を省き、ほかを速める）。ENTER で早送り（「やりなおす」を選んだことにする）
//   段の秒数・紙の大きさ・体力などは resources/gameover/gameover.json（tools/gameover/make_gameover_assets.py）
//   ※ 企画書の P2（ガムテープ）・付箋ヒント・炎のバリエーションはまだ（後回し）
// =====================================================================
#include "engine/math/struct.h"
#include "game/gameover/GameOverScreen.h"
#include "game/gameover/PaperSheet.h"

#include <d3d12.h>
#include <memory>
#include <vector>

class Camera;
class Obj3d;
class RenderTexture;

class GameOverSequence {
public:
    GameOverSequence();
    ~GameOverSequence();

    void Initialize();
    void Finalize();

    // 箱を作る範囲（ステージの地面の範囲）と、地面の上面の高さ
    void SetStageBounds(const Vector3& min, const Vector3& max, float groundTop);

    // 始める。shortVersion＝2回目以降の短縮版
    void Start(Camera* camera, bool shortVersion);
    void Abort(); // 途中でやめる（エディタへ戻った時など）
    void Update(float deltaTime);

    bool IsActive() const{ return phase_ != Phase::None; }
    // 3D のステージを描かない（紙・板だけ）
    bool HidesScene() const;
    // P0 のやられポーズ（足元からの高さ・縦のつぶれ・あおむけの傾き）。ポーズ中でなければ false
    bool DefeatPose(float& outLift, float& outSquash, float& outTilt) const;

    // 体力（♡の数）と、1回のダメージで減る数（gameover.json）
    int HeartCount() const{ return hearts_; }
    int DamagePerHit() const{ return damage_; }

    // シーンへの頼み：チェックポイントの状態へ戻して、再開のカメラを教えてほしい（P4。見えていない間）
    bool ConsumeRestoreRequest();
    void SetResumeCamera(const Vector3& position, const Vector3& rotation, const Matrix4x4& projection);
    // 終わった：操作を返す（やりなおす）
    bool ConsumeFinished();

    // 最終画像ができた所（BaseScene::OnFrameComposed から）。写し取り・前の画面の出し直し
    void OnFrameComposed(ID3D12GraphicsCommandList* commandList, ID3D12Resource* finalTarget);

    void DrawOpaque();
    void DrawUI(); // ゲームオーバーの画面（スプライト）
    void DrawDebugUI();

private:
    enum class Phase { None, Freeze, Reveal, Capture, Crumple, Toss, Screen, Choose, ResumeShot, Unscreen, Rebuild, Leave };
    struct Timing { float freeze = 0.4f, reveal = 0.8f, crumple = 0.8f, toss = 0.5f, screen = 1.0f, rebuild = 0.8f; };

    void LoadSettings();
    void Enter(Phase phase);
    void UpdateInput();
    void ApplyCamera();
    void PlaceSheet(float push);
    void BuildBox();
    void Warmup(); // 初めて使う描き方を、シーンの始めに見えない大きさで描いておく（初回の引っかかりを無くす）
    float PhaseLength(Phase phase) const;
    void Copy(ID3D12GraphicsCommandList* commandList, ID3D12Resource* source, ID3D12Resource* destination);

    // 時間・段
    Phase phase_ = Phase::None;
    int   warmupFrames_ = 3;
    float time_ = 0.0f;
    float speed_ = 1.0f;   // 早送り
    bool  short_ = false;
    Timing full_, shortTiming_;
    int   hearts_ = 12, damage_ = 3;
    float sheetDistance_ = 4.0f, tossDistance_ = 9.0f;
    float revealBack_ = 6.0f, revealUp_ = 3.0f, revealFov_ = 0.18f, boxRim_ = 1.4f;

    // シーンとのやりとり
    Camera* camera_ = nullptr;
    bool restoreRequested_ = false, finished_ = false;
    bool captureRequested_ = false, captured_ = false;
    bool resumeShotRequested_ = false, resumeShotTaken_ = false;
    Vector3 resumePosition_ {}, resumeRotation_ {};
    float   resumeFov_ = 0.78f;

    // カメラ（演出が動かす）
    Vector3 camPos_ {}, camLook_ {};
    float   camFov_ = 0.78f;
    Vector3 startPos_ {};
    float   startFov_ = 0.78f;
    Vector3 forward_ { 0, 0, 1 }, right_ { 1, 0, 0 }, up_ { 0, 1, 0 }; // 写し取った時のカメラの向き
    float   aspect_ = 16.0f / 9.0f;

    // 紙
    PaperSheet sheet_;
    std::unique_ptr<RenderTexture> capture_;   // 死んだ瞬間の絵
    std::unique_ptr<RenderTexture> resume_;    // 再開する所の絵
    std::unique_ptr<RenderTexture> previous_;  // 前のフレームの絵（再開の絵を写すフレームに出し直す）
    bool  previousValid_ = false;
    float sheetWidth_ = 1.0f, sheetHeight_ = 1.0f;
    float push_ = 0.0f;                        // 紙が奥へ飛んでいる距離
    std::unique_ptr<Obj3d> board_;             // 紙の奥の段ボールの板（縮んだ紙のまわりに見える）

    // ゲームオーバーの画面
    GameOverScreen screen_;
    int choice_ = 0; // 0＝やりなおす 1＝ステージをえらぶ

    // 箱（種明かし）
    std::vector<std::unique_ptr<Obj3d>> walls_;
    Vector3 boundsMin_ {}, boundsMax_ {};
    float groundTop_ = 0.0f;
    bool hasBounds_ = false;
};
