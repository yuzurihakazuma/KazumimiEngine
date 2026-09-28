#pragma once
#include "engine/math/struct.h"
#include <memory>

class Obj3d;

// =====================================================================
//  Egg：ヨッシーの卵1個。実体（Obj3d）を持ち、状態で振る舞いを変える。
//   高レベルの状態（Held/Flying/Broken）はステートマシン。
//   「生まれて後ろに並ぶ・追従」は状態ではなく目標位置への補間(lerp)で表現する。
// =====================================================================
enum class EggState {
    Held,    // ヨッシーの後ろで待機（目標スロットへ寄っていく）
    Flying,  // 投げられて飛行中
    Broken,  // 割れた（少ししてから消える）
};

class Egg {
public:
    explicit Egg(const Vector3& birthPos);
    ~Egg();

    // 実体（見た目のObj3d）を持たせる。baseScale=通常時の大きさ。
    void AttachVisual(std::unique_ptr<Obj3d> obj, const Vector3& baseScale);

    void Update(float dt); // 状態に応じた更新＋見た目の更新
    void Draw() const;

    // --- 状態遷移 ---
    void Throw(const Vector3& dir, float speed); // Held → Flying
    void Break();                                // → Broken

    // 並ぶ目標位置（後ろのスロット）。Held の間、ここへ滑らかに寄っていく。
    void SetTarget(const Vector3& t) { target_ = t; }

    EggState State() const { return state_; }
    bool IsHeld()   const { return state_ == EggState::Held; }
    bool IsFlying() const { return state_ == EggState::Flying; }
    bool IsBroken() const { return state_ == EggState::Broken; }
    bool IsDead()   const { return state_ == EggState::Broken && brokenTimer_ <= 0.0f; }

    const Vector3& GetPosition() const { return pos_; }
    void  SetPosition(const Vector3& p) { pos_ = p; prevPos_ = p; }
    float GetRadius() const { return radius_; }

    // 直前の更新で動く前にいた位置（当たり判定を「動いた区間」で取ってすり抜けを防ぐ）
    const Vector3& GetPrevPosition() const { return prevPos_; }

    // 飛行中に壁や地面へ当たった：当たった位置で止め、次の判定で割る。
    //   その場ですぐ割らないのは、同じ区間の手前に敵がいた時に命中を取りこぼさないため
    void StopAtObstacle(const Vector3& hitPos);
    bool HitObstacle() const { return hitObstacle_; }

    // 割れた「瞬間」を1回だけ拾うためのフラグ（パーティクルを弾けさせる用）
    bool JustBroke() const { return justBroke_; }
    void ClearJustBroke() { justBroke_ = false; }

    // 産卵エロージョン演出中は実体メッシュを隠す（SDFの卵が育ちきったら表示に戻す）
    void SetVisualHidden(bool hidden) { visualHidden_ = hidden; }
    bool IsVisualHidden() const { return visualHidden_; }

private:
    EggState state_ = EggState::Held;
    Vector3  pos_ { 0.0f, 0.0f, 0.0f };
    Vector3  prevPos_ { 0.0f, 0.0f, 0.0f }; // 直前の更新で動く前の位置
    bool     hitObstacle_ = false;         // 壁/地面に当たって止まった（次の判定で割れる）
    Vector3  target_ { 0.0f, 0.0f, 0.0f }; // 並ぶ目標（後ろのスロット）
    Vector3  vel_ { 0.0f, 0.0f, 0.0f };    // 飛行中の速度
    float    radius_ = 0.4f;
    float    flyTimer_   = 0.0f;
    float    brokenTimer_ = 0.0f;
    bool     justBroke_   = false;         // この瞬間に割れたか（1フレームだけ true）
    bool     visualHidden_ = false;        // 産卵演出中の実体メッシュ非表示
    float    bornTimer_   = 0.25f;         // 生まれた直後の「ポンッ」と出る演出時間
    float    spinAngle_   = 0.0f;          // 見た目のくるくる回転

    Vector3  baseScale_ { 0.4f, 0.4f, 0.4f };
    std::unique_ptr<Obj3d> obj_;           // 実体（見た目）
};
