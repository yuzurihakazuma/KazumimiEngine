#include "game/worldmap/DiveSequence.h"

#include "engine/utils/Easing.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kBodyCenter = 0.55f;   // 回る軸の高さ（足元から。恐竜の体の真ん中）
    constexpr float kFlipTurns = 2.0f;     // 前転の回数（着地で立った向きに戻る）
    constexpr float kApexAlong = 0.7f;     // 頂点は、立っている所から着地する所までのどこか（0〜1）
    constexpr float kLandSquash = 0.25f;   // 着地でつぶれる時間(秒)
    constexpr float kIrisDelay = 0.12f;    // 着地してから円を閉じ始めるまで(秒)

    Vector3 Lerp(const Vector3& a, const Vector3& b, float t){
        return { a.x + ( b.x - a.x ) * t, a.y + ( b.y - a.y ) * t, a.z + ( b.z - a.z ) * t };
    }
    Vector3 Add(const Vector3& a, const Vector3& b){ return { a.x + b.x, a.y + b.y, a.z + b.z }; }
}

void DiveSequence::Start(const DiveTiming& timing, const Vector3& from, float yaw, const Vector3& target, float endScale,
                         const Vector3& cameraPosition, const Vector3& cameraLook){
    timing_ = timing;
    from_ = from;
    yaw_ = yaw;
    target_ = target;
    endScale_ = endScale;
    cameraStart_ = cameraPosition;
    lookStart_ = cameraLook;
    time_ = 0.0f;
    active_ = true;
    jumpTaken_ = landTaken_ = false;
}

void DiveSequence::SetTarget(const Vector3& target, float endScale){
    target_ = target;
    endScale_ = endScale;
}

void DiveSequence::Update(float deltaTime){
    if ( active_ ) { time_ += deltaTime; }
}

float DiveSequence::EndTime() const{
    return LandTime() + ( std::max )( timing_.absorb, kIrisDelay + timing_.irisTime ) + timing_.irisHold;
}

bool DiveSequence::ConsumeJumped(){
    if ( !active_ || time_ < timing_.crouch || jumpTaken_ ) { return false; }
    jumpTaken_ = true;
    return true;
}

bool DiveSequence::ConsumeLanded(){
    if ( !active_ || time_ < LandTime() || landTaken_ ) { return false; }
    landTaken_ = true;
    return true;
}

DiveSequence::Pose DiveSequence::DinoPose() const{
    const float t1 = timing_.crouch;
    const float t2 = t1 + timing_.jump;
    const float t3 = t2 + timing_.apex;
    const float t4 = LandTime();
    const float t = time_;
    // 頂点：着地する所の手前の上
    const Vector3 flat = Lerp(from_, target_, kApexAlong);
    const Vector3 apex { flat.x, target_.y + timing_.apexHeight, flat.z };

    Vector3 feet = from_;                 // 立った向きの時の足元
    Vector3 stretch { 1.0f, 1.0f, 1.0f }; // 伸び縮み
    float size = 1.0f;                    // 全体の大きさ
    if ( t < t1 ) {
        // 構え：かがむ（縦に縮んで横にふくらむ）
        const float u = Easing::EaseOutQuad(t / timing_.crouch);
        stretch = { 1.0f + 0.15f * u, 1.0f - 0.25f * u, 1.0f + 0.15f * u };
    } else if ( t < t2 ) {
        // ジャンプ：上がるほど遅く。跳んだ直後は縦に伸びる
        const float u = ( t - t1 ) / timing_.jump;
        const Vector3 along = Lerp(from_, apex, Easing::EaseOutSine(u));
        feet = { along.x, from_.y + ( apex.y - from_.y ) * Easing::EaseOutQuad(u), along.z };
        const float s = 0.2f * ( 1.0f - Easing::Clamp01(u * 2.0f) );
        stretch = { 1.0f - s * 0.5f, 1.0f + s, 1.0f - s * 0.5f };
    } else if ( t < t3 ) {
        // 頂点：ふわっと少しだけ上がって止まる
        const float u = ( t - t2 ) / timing_.apex;
        feet = { apex.x, apex.y + 0.12f * std::sin(u * kPi), apex.z };
    } else if ( t < t4 ) {
        // 飛び込み：回りながら落ちて、箱庭と同じ大きさまで縮む
        const float u = ( t - t3 ) / timing_.dive;
        const Vector3 along = Lerp(apex, target_, Easing::EaseInOutSine(u));
        feet = { along.x, apex.y + ( target_.y - apex.y ) * Easing::EaseInQuad(u), along.z };
        size = 1.0f + ( endScale_ - 1.0f ) * Easing::EaseInOutSine(u);
    } else {
        // 着地：箱庭の中のスタートに、小さくなって立つ。少しつぶれて戻る
        feet = target_;
        size = endScale_;
        const float u = Easing::Clamp01(( t - t4 ) / kLandSquash);
        const float squash = 0.3f * std::sin(u * kPi) * ( 1.0f - 0.4f * u );
        stretch = { 1.0f + squash * 0.6f, 1.0f - squash, 1.0f + squash * 0.6f };
    }

    // 前転：ジャンプの途中から着地までに2回転（体の真ん中を軸に）
    float pitch = 0.0f;
    const float flipStart = t1 + timing_.jump * 0.45f;
    if ( t > flipStart && t < t4 ) {
        pitch = 2.0f * kPi * kFlipTurns * Easing::EaseInOutSine(( t - flipStart ) / ( t4 - flipStart ));
    }
    const float center = kBodyCenter * size;
    const Vector3 forward { std::sin(yaw_), 0.0f, std::cos(yaw_) };
    // 体の真ん中から足元への向き（回った分だけ傾く。ピッチ＋は頭が前へ倒れる）
    const Vector3 toCenter { forward.x * center * std::sin(pitch), center * std::cos(pitch), forward.z * center * std::sin(pitch) };

    Pose pose;
    pose.position = { feet.x - toCenter.x, feet.y + center - toCenter.y, feet.z - toCenter.z };
    pose.rotation = { pitch, yaw_, 0.0f };
    pose.scale = { size * stretch.x, size * stretch.y, size * stretch.z };
    return pose;
}

void DiveSequence::CameraPose(Vector3& outPosition, Vector3& outLook) const{
    const float t1 = timing_.crouch;
    const float t2 = t1 + timing_.jump;
    const float t3 = t2 + timing_.apex;
    const float t4 = LandTime();
    const float t = time_;
    // 跳ぶ弧の全体と箱庭が入る、斜め前の位置 → 着地する所へ寄る
    const Vector3 wide = Add(target_, timing_.cameraApex);
    const Vector3 close = Add(target_, timing_.cameraDive);
    const Vector3 wideLook = Add(Lerp(from_, target_, 0.6f), { 0.0f, timing_.apexHeight * 0.8f, 0.0f });
    if ( t < t3 ) {
        // 恐竜のすぐ後ろから始まるので、跳ぶ前に急いで下がる
        const float u = Easing::EaseInOutSine(Easing::Clamp01(t / ( t1 + timing_.jump * 0.5f )));
        outPosition = Lerp(cameraStart_, wide, u);
        outLook = Lerp(lookStart_, wideLook, u);
    } else {
        const float u = Easing::EaseInOutSine(Easing::Clamp01(( t - t3 ) / ( t4 - t3 + timing_.absorb * 0.5f )));
        outPosition = Lerp(wide, close, u);
        outLook = Lerp(wideLook, target_, u);
    }
}

float DiveSequence::IrisRadius() const{
    const float start = LandTime() + kIrisDelay;
    if ( !active_ || time_ < start ) { return 2.0f; }
    const float u = Easing::Clamp01(( time_ - start ) / ( std::max )( timing_.irisTime, 0.01f ));
    return 1.4f * ( 1.0f - Easing::EaseInQuad(u) );
}
