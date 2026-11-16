#include "game/clear/GoalRing.h"

#include "engine/3d/obj/Obj3d.h"
#include "engine/math/Matrix4x4.h"
#include "engine/math/VectorMath.h"
#include "engine/utils/Easing.h"
#include "game/rail/RailField.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr const char* kTopModel = "goalRingTop";
    constexpr const char* kBottomModel = "goalRingBottom";
    constexpr float kEndMargin = 0.3f;      // 輪は、レールの端からこれより内側に置く(m)
    constexpr float kPassSideMargin = 0.15f; // くぐった判定：くぐる所の幅に足す余裕(m)
    constexpr float kPassBelow = 0.6f;       // くぐった判定：足元がこれだけ下でも数える（輪の下のふちは地面に埋まっている）(m)
    constexpr float kMaxStep = 3.0f;         // 1 フレームでこれ以上動いた時は、くぐったと数えない（置き直し・戻された時）(m)
    constexpr float kHopTime = 0.45f;        // ふたが跳ねて戻るまでの、開く時間に対する割合
}

GoalRing::GoalRing() = default;
GoalRing::~GoalRing() = default;

void GoalRing::Initialize(){
    top_ = Obj3d::Create(kTopModel);
    bottom_ = Obj3d::Create(kBottomModel);
    // 薄い板なので、裏からも見えるように
    for ( Obj3d* piece : { top_.get(), bottom_.get() } ) {
        if ( piece ) { piece->SetPipelineType(PipelineType::Object3D_CullNone); }
    }
}

void GoalRing::Place(const RailField& railField, const ClearRing& shape, float ringOffset){
    shape_ = shape;
    placed_ = false;
    if ( !railField.HasGoal() ) { return; }
    const auto& rails = railField.GetRails();
    rail_ = railField.GetGoalRail();
    const SplineRail& rail = rails[rail_];
    const float length = rail.GetLength();
    if ( length <= kEndMargin * 2.0f ) { return; }
    railDistance_ = std::clamp(railField.GetGoalDistance() + ringOffset, kEndMargin, length - kEndMargin);
    position_ = rail.GetPositionByDistance(railDistance_);
    const Vector3 tangent = rail.GetTangentByDistance(railDistance_);
    const Vector3 flat { tangent.x, 0.0f, tangent.z };
    forward_ = VectorMath::NormalizeSafe(flat, { 1.0f, 0.0f, 0.0f });
    yaw_ = std::atan2(forward_.x, forward_.z);
    placed_ = true;
    Close();
}

void GoalRing::Close(){
    opening_ = false;
    openTime_ = 0.0f;
    ApplyLid();
}

void GoalRing::Open(){
    if ( opening_ ) { return; }
    opening_ = true;
    openTime_ = 0.0f;
}

bool GoalRing::CheckPass(const Vector3& previous, const Vector3& current, float& outDirection) const{
    if ( !placed_ ) { return false; }
    const Vector3 move = current - previous;
    if ( VectorMath::LengthSq(move) > kMaxStep * kMaxStep ) { return false; }
    // 輪の面からの、符号付きの距離（レールの進む向きが＋）
    const float before = VectorMath::Dot(previous - position_, forward_);
    const float after = VectorMath::Dot(current - position_, forward_);
    if ( before == after || ( before < 0.0f ) == ( after < 0.0f ) ) { return false; } // 面を横切っていない
    // 横切った点が、くぐる所の中か
    const float t = before / ( before - after );
    const Vector3 crossing = previous + move * t;
    const Vector3 side { forward_.z, 0.0f, -forward_.x };
    const float lateral = VectorMath::Dot(crossing - position_, side);
    const float height = crossing.y - position_.y;
    if ( std::abs(lateral) > shape_.innerHalfWidth + kPassSideMargin ) { return false; }
    if ( height < -kPassBelow || height > shape_.innerHeight ) { return false; }
    outDirection = ( after > before ) ? 1.0f : -1.0f;
    return true;
}

void GoalRing::Update(float deltaTime){
    if ( !placed_ ) { return; }
    if ( opening_ ) { openTime_ += deltaTime; }
    ApplyLid();
    if ( top_ ) { top_->Update(); }
    if ( bottom_ ) { bottom_->Update(); }
}

// ふた：ちょうつがいを軸に、輪の面の中で横へ開く（行き過ぎて戻る）。開き始めにぽんと跳ねる
void GoalRing::ApplyLid(){
    if ( !top_ || !bottom_ ) { return; }
    using namespace MatrixMath;
    const float progress = ( shape_.openTime > 0.0f ) ? Easing::Clamp01(openTime_ / shape_.openTime) : 1.0f;
    const float angle = opening_ ? shape_.openAngle * Easing::EaseOutBack(progress) : 0.0f;
    const float hopProgress = Easing::Clamp01(progress / kHopTime);
    const float hop = opening_ ? shape_.openHop * std::sin(hopProgress * Easing::kPi) * ( 1.0f - progress ) : 0.0f;

    const Matrix4x4 facing = MakeRotateY(yaw_);
    bottom_->SetTranslation(position_);
    bottom_->SetRotation({ 0.0f, yaw_, 0.0f });

    // モデルの中で、ちょうつがい hinge を動かさずに Z（厚みの向き）で回す → その後、レールの向きへ
    const Vector3 hinge { shape_.hinge.x, shape_.hinge.y, 0.0f };
    const Matrix4x4 swing = MakeRotateZ(-angle);
    const Vector3 swungHinge = Transforms(hinge, swing);
    const Vector3 localOffset { hinge.x - swungHinge.x, hinge.y - swungHinge.y + hop, 0.0f };
    const Vector3 offset = Transforms(localOffset, facing);
    top_->SetTranslation(position_ + offset);
    top_->SetRotation(ToEulerXYZ(Multiply(swing, facing)));
}

void GoalRing::Draw(){
    if ( !placed_ ) { return; }
    if ( bottom_ ) { bottom_->Draw(); }
    if ( top_ ) { top_->Draw(); }
}
