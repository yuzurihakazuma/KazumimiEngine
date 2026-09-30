#pragma once
// =====================================================================
//  TitleDino：タイトル画面の恐竜の見た目（プレイヤーと同じモデル）。
//   位置と向きは本編と同じ Player が決める（レールの上を走る・乗り換える・ジャンプする）。
//   ここはその Player に合わせてモデルを動かし、タイトルだけの動き（ベロで的を選ぶ・卵を投げる）を足す
//   ・通常：歩く／止まる／ジャンプ／ふんばり のクリップを Player の状態から選ぶ
//   ・ベロ：的へベロを伸ばす（届いた瞬間を ConsumeTongueHit で受け取る）
//   ・投げ：卵を投げる（手を離す瞬間を ConsumeThrowRelease で受け取る）
// =====================================================================
#include "engine/math/struct.h"

#include <cstdint>
#include <memory>

class Player;
class SkinnedObj3d;

class TitleDino {
public:
    TitleDino();
    ~TitleDino();

    void Initialize(uint32_t envMapSrv);
    void Finalize();
    // player の位置・向き・接地状態に合わせて動かす
    void Update(const Player& player, float deltaTime);
    void Draw();

    void Appear();                     // ぴょこっと現れる（登場の最初）
    void ShowImmediately();            // 現れる演出を飛ばす
    void ShootTongue(float distance);  // 向いている方向へ、distance(m) 先までベロを伸ばす
    void StartThrow();                 // 卵を投げる動き
    void CancelAction();               // ベロ・投げを途中でやめる
    bool IsBusy() const{ return action_ != Action::None; }
    bool ConsumeTongueHit();           // ベロが届いた瞬間に1回だけ true
    bool ConsumeThrowRelease();        // 卵が手を離れる瞬間に1回だけ true

    Vector3 GetHandPosition() const;   // 手（卵を持つ位置）

private:
    enum class Action { None, Tongue, Throw };

    void SelectMoveClip(const Player& player, float deltaTime);

    std::unique_ptr<SkinnedObj3d> model_;
    Action  action_ = Action::None;
    float   actionTime_ = 0.0f;   // ベロ・投げを始めてからの秒数
    float   tongueStretch_ = 1.0f;
    bool    tongueHitPending_ = false;
    bool    tongueHitFired_ = false;
    bool    throwReleasePending_ = false;
    bool    throwReleaseFired_ = false;

    bool    visible_ = false;
    float   appearTime_ = 0.0f;   // 現れ始めてからの秒数
    Vector3 position_ {};
    Vector3 prevPosition_ {};     // 前フレームの位置（歩く速さを見た目から測る）
    bool    prevPositionValid_ = false;
};
