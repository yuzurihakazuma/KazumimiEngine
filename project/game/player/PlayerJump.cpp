#include "game/player/PlayerJump.h"

#include <algorithm>
#include <cmath>

void PlayerJump::Reset(){
    height = 0.0f;
    velocity = 0.0f;
    grounded = true;
    flutterTime = 0.0f;
}

void PlayerJump::Land(float groundY){
    height       = groundY;
    velocity     = 0.0f;
    grounded     = true;
    flutterTime  = 0.0f;
    flutterCount = 0;   // ふんばりの減衰も着地でリセット
    fluttering   = false;
}

void PlayerJump::UpdateVelocity(bool pressed, bool held, bool locked, float dt){
    if ( grounded && !locked && pressed ) {
        Launch(power);
        flutterTime  = kFlutterTime; // 滞空budgetを補充
        flutterCount = 0;            // ふんばり回数リセット（この後の長押しが1回目＝フル性能）
    } else if ( !grounded && pressed ) {
        // 空中でSPACEを押し直すと何度でもふんばれる（押し直しごとに上がる高さは減っていく）
        flutterTime = kFlutterTime;
        ++flutterCount;
    }
    // ふんばり中はSPACE長押しで「弱い重力＋ゆるい上昇」へなめらかに移行。目標の上向き速度は
    // 回数ごとに 0.65 倍ずつ減衰（1回目フル → 2回目65% → 3回目42%...）。それ以外は通常重力
    float target = kFlutterTarget * std::pow(kFlutterDecay, ( float ) flutterCount);
    fluttering = StepFlutterOrGravity(velocity, flutterTime, target, gravity, held && !grounded, dt);
}

bool PlayerJump::StepFlutterOrGravity(float& upVelocity, float& flutterTimeLeft, float target,
                                      float gravityAccel, bool held, float dt){
    if ( held && flutterTimeLeft > 0.0f && upVelocity < target ) {
        upVelocity += ( target - upVelocity ) * ( std::min )( kFlutterEase * dt, 1.0f );
        flutterTimeLeft -= dt;
        return true;
    }
    upVelocity -= gravityAccel * dt;
    return false;
}
