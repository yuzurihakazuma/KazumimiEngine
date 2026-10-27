#pragma once
// =====================================================================
//  GameOverSequence：ゲームオーバー演出の進行（ゲームオーバー演出 企画・仕様書 Rev 1.1）。
//   P0 止まる        … ゲームの時間を止めて「ポン」。恐竜はやられポーズ
//   P1 種明かし      … カメラが手前へ引き、ステージが段ボール箱の中のジオラマだったと分かる（箱はステージの範囲から自動で作る）
//   P4 写し取る      … その瞬間の画面を1回だけレンダーターゲットへ写し、3D を隠して「1枚の紙」に貼り替える（見た目は途切れない）
//   P3/P5 丸める     … 画面そのものが、アイリスアウトのようにふちから真ん中へ丸まって、小さな紙玉になる（グシャッ）。
//                      まわりは紫のフェルト（ゲームオーバーの画面の地）
//   開く             … 紙玉がポンと弾んで、開くとゲームオーバーの看板（板）になっている
//   P7 ゲームオーバーの画面 … 看板に「GAME OVER」「やりなおす」「ステージをえらぶ」が出る（GameOverScreen）
//   P8 組み上げ      … 「やりなおす」：文字とボタンがしまわれ、看板が真ん中へ丸まって紙玉になり、
//                      開くとチェックポイント（スタート）の画面が画面いっぱいに広がって、そのまま再開
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
    // P0 のやられの演技（くらくら → ぺたん）。演技中でなければ false
    struct DefeatPoseData {
        float lift = 0.0f;   // 足元からの高さ(m)
        float squash = 1.0f; // 縦のつぶれ（1＝そのまま）
        float pitch = 0.0f;  // あおむけに倒れる角度(rad)
        float roll = 0.0f;   // 左右のふらつき(rad)
        float spin = 0.0f;   // 画面から見て横向きへ回る割合（0＝元の向き 1＝横向き）。くらくらの間に回る
        float wobble = 0.0f; // 回りながらの向きのふらつき(rad)
    };
    // とどめを受けた瞬間の仰け反りの角度(rad。マイナス＝後ろへ)。GameOverFlow がスローの間この形にし、演技はここから始まる
    static constexpr float kBendPitch = -0.4f;
    bool DefeatPose(DefeatPoseData& out) const;
    // 恐竜の頭の位置（毎フレーム。星はこのまわりを回る）
    void SetHeadPosition(const Vector3& head){ head_ = head; }

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
    void DrawTransparent(); // やられの演技の星（不透明を描き終えた後）
    void DrawUI(); // ゲームオーバーの画面（スプライト）
    void DrawDebugUI();

private:
    enum class Phase { None, Freeze, Reveal, Capture, Crumple, Open, Screen, Choose, ResumeShot, Unscreen, Fold, Rebuild, Leave };
    struct Timing { float freeze = 0.4f, reveal = 0.8f, crumple = 0.8f, open = 0.7f, screen = 0.8f, rebuild = 0.8f; };

    void LoadSettings();
    void Enter(Phase phase);
    void UpdateInput();
    void ApplyCamera();
    // 紙を、写し取った時のカメラの前（画面の真ん中）に、幅 width・高さ height(m) で置く
    void PlaceSheet(float width, float height);
    void PlaceSheetFull();   // 画面ぴったり
    void PlaceSheetBoard();  // ゲームオーバーの看板（板）と同じ所・同じ大きさ
    // 紙玉の形：amount＝丸まり具合、spin＝回った角度。半径は画面の高さに対して決まった大きさ
    void SetBall(float amount, float spin);
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
    float sheetDistance_ = 4.0f;
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
    uint32_t boardSrv_ = 0;                    // ゲームオーバーの看板（板）の絵
    std::unique_ptr<Obj3d> board_;             // 紙の奥の紫のフェルト（ゲームオーバーの画面の地と同じ絵。画面ぴったり）

    // やられの演技：頭の上でくるくる回るフェルトの星
    void UpdateStars(float deltaTime);
    std::vector<std::unique_ptr<Obj3d>> stars_;
    Vector3 head_ {};
    float   starAngle_ = 0.0f;
    float   starSize_ = 0.0f;  // 星の大きさ（出てくる途中は小さい）
    float   defeatPrev_ = 0.0f;               // 前のフレームの、演技の進み（音を1回だけ鳴らす）

    // ゲームオーバーの画面
    GameOverScreen screen_;
    int choice_ = 0; // 0＝やりなおす 1＝ステージをえらぶ

    // 箱（種明かし）
    std::vector<std::unique_ptr<Obj3d>> walls_;
    Vector3 boundsMin_ {}, boundsMax_ {};
    float groundTop_ = 0.0f;
    bool hasBounds_ = false;
};
