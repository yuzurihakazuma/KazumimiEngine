#pragma once
#include "engine/math/struct.h"
#include <array>
#include <memory>
#include <cstdint>

class Player;
class EnemyManager;
class EggSystem;
class Camera;
class Sprite;
class EggCombo;

// =====================================================================
//  AimThrowController：ヨッシー風の卵投げ（構え→狙い→投擲）。
//   ・Q長押しで構え、矢印キーで「画面上のカーソル」を動かす（上=上/右=右で直感的）
//   ・カーソルが敵に画面上で重なるとロックオン（粘りつき）。Qを離すと投擲
//   ・ロック中はその敵へ速い弾 / 未ロックはカーソルの奥へアンプロジェクトして投げる
//  見た目（卵コンボの手ごたえ 企画書 3）：紙を切り抜いた照準（輪＋4つの切れ込み＋紙の点）が少し遅れてカーソルを追い、
//   ロックするとその物に吸い付いて大きさを合わせ、切れ込みが内側へすぼまる。狙っている物そのものが光る（背景も敵も同じ）。
//   投げた瞬間に照準はぱっと開いて消える。デバッグの線と球は「当たり判定を表示」の時だけ
//  内部は Idle / Aiming の小さなステートマシン。
//  シーンから分離した理由：入力・カーソルUI・照準・投擲が一体の独立した操作系で、
//  他シーンでも同じ組み合わせで再利用できる。
// =====================================================================
class AimThrowController {
public:
    AimThrowController();
    ~AimThrowController(); // unique_ptr<Sprite> のため cpp 側で定義

    // 照準のスプライトを作る（resources/ui/aim_*.png。tools/hud/make_aim_assets.py）
    void Initialize();
    // 狙いの線・球（DebugDraw）を出すか（エディタの「当たり判定を表示」）
    void SetDebugVisible(bool visible){ debugVisible_ = visible; }

    // 毎フレーム（Play中のみ）呼ぶ
    void Update(Player& player, EnemyManager& enemies, EggSystem& eggs, Camera* camera, float dt);

    // 構え中だけカーソルを描画（SpriteCommon::PreDraw 済みの状態で呼ぶ）
    void DrawSprite();

    // 構えを解除して通常状態へ（モード切替時などに呼ぶ）
    void Reset(){ state_ = State::Idle; aimLocked_ = false; depthLocked_ = false; burstT_ = -1.0f; reticleShown_ = false; }
    // 奥投げ：カーソルを背景の木・柵などに合わせるとロックして、奥へ投げる（EggCombo）
    void SetCombo(EggCombo* combo){ combo_ = combo; }

    bool IsAiming() const { return state_ == State::Aiming; }

    // 直前にキャンセル（X）されたか。読むとフラグは消える（投げモーションを出さない判定用）
    bool ConsumeCanceled(){ bool wasCanceled = canceled_; canceled_ = false; return wasCanceled; }

    // 今の狙い方向のヨー角（構え中、プレイヤーの向きをこれに合わせる）
    float GetAimYaw() const{ return aimYaw_; }

    // ノードエディタの「→ ゲーム値」用（投げ初速を外から調整できる）
    float* ThrowSpeedNormalPtr(){ return &throwSpeedNormal_; }
    float* ThrowSpeedLockPtr(){ return &throwSpeedLock_; }

private:
    // 構えのステート（Idle=通常 / Aiming=構え中）
    enum class State { Idle, Aiming };
    State state_ = State::Idle;

    float cursorX_ = 0.0f;   // 狙いカーソルの画面位置X(px)
    float cursorY_ = 0.0f;   // 狙いカーソルの画面位置Y(px)
    bool  aimLocked_ = false; // 敵にロックオン中か（カーソルの色/サイズ変更）
    bool  depthLocked_ = false; // 背景の物にロック中か（奥投げ。カーソルは水色）
    int   depthTarget_ = -1;
    float depthRingSize_ = 96.0f;
    EggCombo* combo_ = nullptr;
    bool  canceled_ = false;  // 直前にXでキャンセルしたか（1回読まれたら消えるフラグ）
    float aimYaw_ = 0.0f;     // 今の狙い方向のヨー角（プレイヤーの向き合わせ用）
    float aimAxisYaw_ = 0.0f; // 構え開始時のレール軸の向き（狙い中は固定）

    float throwSpeedNormal_ = 13.0f; // 通常の投げ初速 (m/s)
    float throwSpeedLock_   = 22.0f; // ロックオン時の投げ初速 (m/s)

    float pulseT_ = 0.0f; // カーソルの脈動用タイマー（視認性向上）

    bool  debugVisible_ = false;

    // 照準（画面の画素）。カーソルを少し遅れて追う
    void UpdateReticle(float targetX, float targetY, float targetSize, bool locked, float dt, float uiScale, float dotX, float dotY);
    void PlaceReticle(float uiScale, float open, float alpha);
    bool  reticleShown_ = false;
    bool  ringVisible_ = true;      // 輪を出すか（ロック中・投げた後は出さない）
    float reticleX_ = 0.0f, reticleY_ = 0.0f;
    float dotX_ = 0.0f, dotY_ = 0.0f;   // 真ん中の点：背景の物をロック中は「当てる所」（物の真ん中とは限らない）
    int   bounceCount_ = 0;             // はじける向きの点（このフレームに出す数）
    float reticleSize_ = 110.0f;    // 輪の直径（1080p の画素）
    float lockAmount_ = 0.0f;       // 0＝ロックしていない → 1＝吸い付いた（切れ込みがすぼまる）
    float burstT_ = -1.0f;          // 投げた瞬間からの秒（-1＝出していない）。照準がぱっと開いて消える
    float uiScale_ = 1.0f;
    int   trailCount_ = 0;          // 卵の通る道の紙の点（このフレームに出す数）
    std::unique_ptr<Sprite> ringSprite_;
    std::array<std::unique_ptr<Sprite>, 4> notchSprites_;
    std::unique_ptr<Sprite> dotSprite_;
    std::array<std::unique_ptr<Sprite>, 4> trailSprites_;
    std::array<std::unique_ptr<Sprite>, 3> bounceSprites_; // 当てた物がはじける向き（物の真ん中から外へ並ぶ小さな点）
};
