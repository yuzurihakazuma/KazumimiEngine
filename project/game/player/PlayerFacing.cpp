#include "game/player/PlayerFacing.h"
#include "engine/math/VectorMath.h"

#include <algorithm>
#include <cmath>

using namespace VectorMath;

void PlayerFacing::Reset(){
    groundTangent_ = { 0.0f, 0.0f, 1.0f }; // 坂なし（次に地面を歩いたフレームで決まり直す）
}

void PlayerFacing::UpdateOnRail(const Vector3& tangent, float moveSign, float dt, float& yaw){
    groundTangent_ = tangent;
    // 目標の更新は動いている間だけだが、回頭そのものは止まっていても続ける
    //   （途中でキーを離すと横向きのまま固まり、レール上では本来向かない方向を向いてしまう問題の対策）
    if ( overrideActive_ ) {
        targetYaw_ = overrideYaw_; // 卵の構え中：狙い方向を向く（移動より優先）
    } else if ( Length(tangent) > 0.001f && moveSign != 0.0f && !turnLocked_ ) {
        targetYaw_ = std::atan2(tangent.x * moveSign, tangent.z * moveSign);
    }
    const float kPi = 3.14159265f;
    float yawDiff = targetYaw_ - yaw;
    while ( yawDiff >  kPi ) yawDiff -= 2.0f * kPi; // 最短弧（+350°回らず -10° で済ませる）
    while ( yawDiff < -kPi ) yawDiff += 2.0f * kPi;
    yaw += yawDiff * ( std::min )( 18.0f * dt, 1.0f ); // 反転(180°)も素早く向き切る
    while ( yaw >  kPi ) yaw -= 2.0f * kPi; // 値が無限に育たないよう正規化
    while ( yaw < -kPi ) yaw += 2.0f * kPi;
}

void PlayerFacing::FaceVelocity(const Vector3& velocity, float& yaw) const{
    float horizSpeed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
    if ( horizSpeed > 0.1f ) { yaw = std::atan2(velocity.x, velocity.z); }
}

Vector3 PlayerFacing::FacingAlongGround(float yaw, bool inAir) const{
    Vector3 facing = { std::sin(yaw), 0.0f, std::cos(yaw) };
    if ( inAir ) return facing; // レールを離れている：坂は無い
    float horizontal = std::sqrt(groundTangent_.x * groundTangent_.x + groundTangent_.z * groundTangent_.z);
    if ( horizontal < 1e-3f ) return facing; // 真上/真下へ向かうレール：坂として扱わない
    // レールの向きのうち、向いている方と同じ向きの側の坂を使う（後ろ向きなら下り坂になる）
    float alongFacing = ( groundTangent_.x * facing.x + groundTangent_.z * facing.z ) / horizontal;
    if ( std::abs(alongFacing) < 0.3f ) return facing; // 道を横切る向き（振り向いている途中など）
    float slope = ( groundTangent_.y / horizontal ) * ( ( alongFacing >= 0.0f ) ? 1.0f : -1.0f );
    Vector3 dir = { facing.x, slope, facing.z };
    float length = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    return { dir.x / length, dir.y / length, dir.z / length };
}
