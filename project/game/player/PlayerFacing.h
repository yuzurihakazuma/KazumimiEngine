#pragma once
// =====================================================================
//  PlayerFacing：プレイヤーの向き（yaw）。
//   ・レール上：実際に進んでいる方向（接線×進行符号）を「目標角」として覚え、
//     最短弧でなめらかに回頭する。キーを離しても回頭は続けて必ず向き切る
//   ・ベロを出している間は振り向かない（TurnLocked）/ 卵の構え中は狙い方向を向く（Override）
//   ・空中：飛んでいる方向を向く
//   足元の道の坂も覚えておき、坂道で前へ物を出す時の向き（FacingAlongGround）に使う
// =====================================================================
#include "engine/math/struct.h"

class PlayerFacing {
public:
    void Reset();

    void SetTurnLocked(bool locked){ turnLocked_ = locked; }
    void SetOverride(bool active, float yaw){ overrideActive_ = active; overrideYaw_ = yaw; }

    // レール上：tangent=足元の接線 / moveSign=進行符号(0=停止)。yaw を目標へ近づける
    void UpdateOnRail(const Vector3& tangent, float moveSign, float dt, float& yaw);
    // 空中：水平速度の方向を向く（ほぼ止まっていたら向きは変えない）
    void FaceVelocity(const Vector3& velocity, float& yaw) const;

    // 足元の道の向き（着地した瞬間など、UpdateOnRail の前に決まる時用）
    void SetGroundTangent(const Vector3& tangent){ groundTangent_ = tangent; }
    // 向いている方向に足元の道の坂を足した向き（長さ1）。空中・道を横切る向きなら水平のまま
    Vector3 FacingAlongGround(float yaw, bool inAir) const;

    // 空中へ出た時、進行符号が0でも進む向きが要る時に使う「向こうとしている向き」
    float TargetYaw() const { return targetYaw_; }

private:
    float   targetYaw_ = 0.0f;       // 向きの目標角
    bool    turnLocked_ = false;     // true=目標を更新しない（ベロを出している間）
    bool    overrideActive_ = false; // true=向きを外部指定に合わせる（卵の構え中の狙い方向）
    float   overrideYaw_ = 0.0f;
    Vector3 groundTangent_ { 0.0f, 0.0f, 1.0f }; // 最後にレールの上にいた時の足元の接線（坂の向き）
};
