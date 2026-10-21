#include "game/worldmap/DiveSequence.h"

#include "engine/utils/Easing.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    Vector3 Lerp(const Vector3& a, const Vector3& b, float t){
        return { a.x + ( b.x - a.x ) * t, a.y + ( b.y - a.y ) * t, a.z + ( b.z - a.z ) * t };
    }
}

void DiveSequence::Start(const DiveTiming& timing, const Vector3& from, float yaw, const Vector3& center,
                         const Vector3& cameraPosition, const Vector3& cameraLook){
    timing_ = timing;
    from_ = from;
    yaw_ = yaw;
    center_ = center;
    // 頂点：箱庭の真ん中の上、少し手前（真上からだと飛び込む向きが見えにくい）
    const Vector3 forward { std::sin(yaw), 0.0f, std::cos(yaw) };
    apex_ = { center.x - forward.x * 0.5f, center.y + timing.apexHeight, center.z - forward.z * 0.5f };
    // 頭から向かう角度：体の上の向き (0,cos,sin)（ピッチ θ。Rx→Ry）を「頂点→真ん中」に合わせる
    const Vector3 toCenter { center.x - apex_.x, center.y - apex_.y, center.z - apex_.z };
    const float along = toCenter.x * forward.x + toCenter.z * forward.z;
    divePitch_ = std::atan2(along, toCenter.y);
    cameraStart_ = cameraPosition;
    lookStart_ = cameraLook;
    time_ = 0.0f;
    active_ = true;
    jumped_ = absorbed_ = jumpTaken_ = absorbTaken_ = false;
}

void DiveSequence::Update(float deltaTime){
    if ( !active_ ) { return; }
    time_ += deltaTime;
    if ( time_ >= timing_.crouch ) { jumped_ = true; }
    if ( time_ >= timing_.crouch + timing_.jump + timing_.apex + timing_.dive ) { absorbed_ = true; }
}

float DiveSequence::EndTime() const{
    const float absorbStart = timing_.crouch + timing_.jump + timing_.apex + timing_.dive;
    return absorbStart + ( std::max )( timing_.absorb, timing_.irisTime ) + timing_.irisHold;
}

bool DiveSequence::ConsumeJumped(){
    if ( !jumped_ || jumpTaken_ ) { return false; }
    jumpTaken_ = true;
    return true;
}

bool DiveSequence::ConsumeAbsorbed(){
    if ( !absorbed_ || absorbTaken_ ) { return false; }
    absorbTaken_ = true;
    return true;
}

DiveSequence::Pose DiveSequence::DinoPose() const{
    Pose pose;
    pose.position = from_;
    pose.rotation = { 0.0f, yaw_, 0.0f };
    const float t1 = timing_.crouch;
    const float t2 = t1 + timing_.jump;
    const float t3 = t2 + timing_.apex;
    const float t4 = t3 + timing_.dive;
    const float t = time_;

    // 前転：ジャンプの後半から頂点の終わりまでに、1回転＋頭を下へ
    const float flipStart = t1 + timing_.jump * 0.5f;
    if ( t > flipStart ) {
        const float u = Easing::EaseInOutSine(Easing::Clamp01(( t - flipStart ) / ( t3 - flipStart )));
        pose.rotation.x = ( 2.0f * kPi + divePitch_ ) * u;
    }

    if ( t < t1 ) {
        // 構え：かがむ（縦に縮んで横にふくらむ）
        const float u = Easing::EaseOutQuad(t / timing_.crouch);
        pose.scale = { 1.0f + 0.15f * u, 1.0f - 0.25f * u, 1.0f + 0.15f * u };
    } else if ( t < t2 ) {
        // ジャンプ：頂点へ（上がるほど遅く）。跳んだ直後は縦に伸びる
        const float u = ( t - t1 ) / timing_.jump;
        const Vector3 flat = Lerp(from_, apex_, Easing::EaseOutSine(u));
        pose.position = { flat.x, from_.y + ( apex_.y - from_.y ) * Easing::EaseOutQuad(u), flat.z };
        const float stretch = 0.2f * ( 1.0f - Easing::Clamp01(u * 2.0f) );
        pose.scale = { 1.0f - stretch * 0.5f, 1.0f + stretch, 1.0f - stretch * 0.5f };
    } else if ( t < t3 ) {
        // 頂点：ほんの少し浮いて止まる
        const float u = ( t - t2 ) / timing_.apex;
        pose.position = { apex_.x, apex_.y + 0.15f * std::sin(u * kPi), apex_.z };
    } else if ( t < t4 ) {
        // 飛び込み：頭から真ん中へ。加速しながら縮む
        const float u = Easing::EaseInQuad(( t - t3 ) / timing_.dive);
        pose.position = Lerp(apex_, center_, u);
        const float s = 1.0f + ( timing_.endScale - 1.0f ) * u;
        pose.scale = { s, s, s };
    } else {
        pose.position = center_;
        pose.visible = false; // 吸い込まれた
    }
    return pose;
}

void DiveSequence::CameraPose(Vector3& outPosition, Vector3& outLook) const{
    const float t1 = timing_.crouch;
    const float t2 = t1 + timing_.jump;
    const float t3 = t2 + timing_.apex;
    const float t4 = t3 + timing_.dive;
    const float t = time_;
    const Vector3 apexCamera { center_.x + timing_.cameraApex.x, center_.y + timing_.cameraApex.y, center_.z + timing_.cameraApex.z };
    const Vector3 diveCamera { center_.x + timing_.cameraDive.x, center_.y + timing_.cameraDive.y, center_.z + timing_.cameraDive.z };
    const Pose dino = DinoPose();
    // 見る所：箱庭と恐竜の間（跳んでいる恐竜と、飛び込む先の箱庭が両方入る）。飛び込むほど箱庭へ
    const Vector3 between = Lerp(center_, dino.position, 0.45f);

    if ( t < t3 ) {
        // 斜め前の高い所へ下がりながら、跳ぶ弧の全体を見る
        outPosition = Lerp(cameraStart_, apexCamera, Easing::EaseInOutSine(Easing::Clamp01(t / t3)));
        outLook = Lerp(lookStart_, between, Easing::EaseInOutSine(Easing::Clamp01(t / ( t1 + timing_.jump * 0.6f ))));
    } else {
        // 飛び込み：箱庭へ急に寄って、真ん中を見る
        const float u = Easing::EaseInQuad(Easing::Clamp01(( t - t3 ) / ( t4 - t3 )));
        outPosition = Lerp(apexCamera, diveCamera, u);
        outLook = t >= t4 ? center_ : Lerp(between, center_, u);
    }
}

float DiveSequence::IrisRadius() const{
    const float absorbStart = timing_.crouch + timing_.jump + timing_.apex + timing_.dive;
    if ( !active_ || time_ < absorbStart ) { return 2.0f; }
    const float u = Easing::Clamp01(( time_ - absorbStart ) / ( std::max )( timing_.irisTime, 0.01f ));
    return 1.4f * ( 1.0f - Easing::EaseInQuad(u) );
}
