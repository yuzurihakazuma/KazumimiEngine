#pragma once
// =====================================================================
//  GameOverFlow：体力（♡）とゲームオーバーの流れを1つにまとめた物。ゲームシーンはこれを決まった所で呼ぶだけ。
//   ・♡（HeartRing）：敵に弾かれた・穴に落ちたら減る。残っていれば落ちた時はスタートへ戻す（円を閉じて開く）
//   ・最後の♡が無くなったら：仰け反ってスロー → 地面に着くのを待つ（空中ならそのまま落ちる）→ ゲームオーバー演出（GameOverSequence）
//     演出の間はゲームの時間を止める
//   ・演出の途中（画面を紙に写して見えていない間）に、ステージをスタートの状態へ戻し、再開のカメラを求める
//   ・やられポーズ・2回目からの短縮版・確認用のキー（環境変数 CG2_DEBUG_KEYS で F7＝ゲームオーバー / F6＝ダメージ）
//   ・インスペクターの「体力・ゲームオーバー」
//  ステージの物（敵・コイン・ブロック・卵など）を戻す中身は、シーンが Context::resetStage で渡す
// =====================================================================
#include "engine/math/struct.h"
#include "game/gameover/GameOverSequence.h"
#include "game/hud/HeartRing.h"

#include <d3d12.h>
#include <functional>
#include <memory>

class Camera;
class Player;
class PlayerAvatar;
class PlayCameraController;
class PlayScreenEffects;
class RailField;
class RenderTexture;
class StageBackdrop;

class GameOverFlow {
public:
    // シーンの部品（持たない。シーンの方が長生き）
    struct Context {
        Player*               player = nullptr;
        PlayerAvatar*         avatar = nullptr;
        RailField*            railField = nullptr;
        PlayCameraController* camCtrl = nullptr;
        PlayScreenEffects*    screenFx = nullptr;
        Camera*               camera = nullptr;
        float                 farClip = 200.0f;          // 再開のカメラの遠くの見える限界（プレイ中のカメラと同じに）
        std::function<void()> resetStage;                 // 敵・コイン・？ブロック・卵・構え・舌を、プレイ開始の時と同じに戻す
    };

    GameOverFlow();
    ~GameOverFlow();

    // backdrop＝種明かしの箱を作るためのステージの地面
    void Initialize(const Context& context, const StageBackdrop& backdrop);
    void Finalize();

    void OnPlayStart();  // プレイを始めた：♡を満タンに（出さない）
    void OnEditStart();  // エディタへ戻った：演出をやめて、♡を満タンに

    // プレイ中の進行のはじめ。演出中（ゲームの時間を止めている）なら true → シーンはそのフレームのゲームを進めない
    bool UpdateBeforeGame(bool introActive);
    // 当たり判定の後：ぶつかった・落ちた → ♡が減る。ゲームオーバーになったら true
    bool UpdateAfterCombat();
    // 見た目（恐竜の見た目の更新の前に呼ぶ）：やられポーズと、♡の輪の位置
    void UpdateVisuals(bool playing, const Vector3& playerPos);

    bool IsActive() const{ return sequence_.IsActive(); } // 演出中（とどめのスロー・着地待ちの間はまだ false。ゲームは進む）
    bool HidesScene() const{ return sequence_.HidesScene(); } // 紙に貼り替えている間（3D のステージを描かない）

    void DrawOpaque(){ sequence_.DrawOpaque(); }
    void DrawTransparent(){ sequence_.DrawTransparent(); } // やられの演技の星
    // スプライト：演出中はゲームオーバーの画面を描いて true（HUD は出さない）。そうでなければ ♡ を描いて false
    bool DrawUI(bool playing);
    void OnFrameComposed(ID3D12GraphicsCommandList* commandList, RenderTexture* finalTarget);
    void DrawInspector(bool playing);

private:
    void StartGameOver();
    void RestoreCheckpoint(); // チェックポイント（今はスタート）へ戻す
    void Damage(bool fellOut = false);
    void BeginDying();        // 最後の♡：仰け反ってスロー、着地を待つ
    void UpdateDying();
    void EndDying();          // 時間の速さを元に戻す

    Context context_;
    HeartRing hearts_;
    GameOverSequence sequence_;
    std::unique_ptr<Camera> resumeCamera_; // 再開する所のカメラを求める（画面には出さない）
    bool defeatPoseApplied_ = false;
    bool  dying_ = false;     // とどめを受けてからゲームオーバー演出までの間（スロー → 着地待ち）
    float dyingTime_ = 0.0f;  // その経過(実時間の秒)
    static inline int gameOverCount_ = 0;   // このステージでのゲームオーバーの回数（2回目から短縮版）
};
