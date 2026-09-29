#pragma once
// =====================================================================
//  PlayerKnockback：敵にぶつかった時のノックバックと無敵時間。
//   ぶつかると敵と反対側へ小さく跳ねながら弾かれ、その間は操作できない。
//   その後もしばらく無敵（連続で当たり続けない。見た目の点滅は PlayerAvatar）
// =====================================================================
#include "engine/math/struct.h"

class PlayerKnockback {
public:
    static constexpr float kKnockTime      = 0.25f; // 弾かれて操作できない時間(秒)
    static constexpr float kInvincibleTime = 1.2f;  // ぶつかった後の無敵時間(秒)
    static constexpr float kHopVelocity    = 4.5f;  // 弾かれる時の小さな跳ね(m/s)
    static constexpr float kAirSpeed       = 4.0f;  // 空中で弾かれた時の水平速度(m/s)
    static constexpr float kRailSpeed      = 5.0f;  // レール上で押し戻される速さ(m/s)

    void Reset(){ knockTimer_ = 0.0f; invincibleTimer_ = 0.0f; }

    // awayDir=敵→自分の方向。水平成分が無い（真上/真下で重なった）時は facingYaw の逆へ弾く
    void Start(const Vector3& awayDir, float facingYaw);

    void TickInvincible(float dt){ if ( invincibleTimer_ > 0.0f ) { invincibleTimer_ -= dt; } }
    void TickKnock(float dt){ if ( knockTimer_ > 0.0f ) { knockTimer_ -= dt; } }

    bool IsKnocked() const { return knockTimer_ > 0.0f; }
    bool IsInvincible() const { return invincibleTimer_ > 0.0f; }
    const Vector3& Direction() const { return knockDir_; } // 弾かれる水平方向（単位ベクトル）

private:
    float   knockTimer_ = 0.0f;
    float   invincibleTimer_ = 0.0f;
    Vector3 knockDir_ { 0.0f, 0.0f, 0.0f };
};
