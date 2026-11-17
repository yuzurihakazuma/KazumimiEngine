#include "game/clear/GoalGate.h"

#include "engine/3d/obj/Obj3d.h"
#include "engine/math/MathUtil.h"
#include "engine/math/Matrix4x4.h"
#include "engine/math/VectorMath.h"
#include "engine/utils/Easing.h"
#include "game/rail/RailField.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr const char* kGateModel = "goalGate";
    constexpr const char* kEggBottomModel = "goalEggBottom";
    constexpr const char* kEggTopModel = "goalEggTop";
    constexpr const char* kTapeLeftModel = "goalTapeLeft";
    constexpr const char* kTapeRightModel = "goalTapeRight";
    constexpr float kEndMargin = 0.3f;       // 門は、レールの端からこれより内側に置く(m)
    constexpr float kPassSideMargin = 0.15f; // くぐった判定：くぐる所の幅に足す余裕(m)
    constexpr float kPassBelow = 0.6f;       // くぐった判定：足元がこれだけ下でも数える(m)
    constexpr float kMaxStep = 3.0f;         // 1 フレームでこれ以上動いた時は、くぐったと数えない（置き直し・戻された時）(m)
    constexpr float kHopShare = 0.45f;       // ふたが跳ねて戻るまでの、開く時間に対する割合
    constexpr float kTapeShare = 0.6f;       // テープが垂れ切るまでの、開く時間に対する割合
    constexpr float kEggRadius = 0.6f;       // 卵の真ん中は、ちょうつがいの高さから少し下（紙吹雪を出す所）
}

GoalGate::GoalGate() = default;
GoalGate::~GoalGate() = default;

void GoalGate::Initialize(){
    gate_ = Obj3d::Create(kGateModel);
    eggBottom_ = Obj3d::Create(kEggBottomModel);
    eggTop_ = Obj3d::Create(kEggTopModel);
    tapeLeft_ = Obj3d::Create(kTapeLeftModel);
    tapeRight_ = Obj3d::Create(kTapeRightModel);
    // 紙の花・旗・テープも厚みのある形（表と裏の面がある）なので、ふつうの描き方のまま。
    //   段ボールの面はテクスチャをくり返すので、Obj3d が自動でくり返しの描き方（WrapUV）にする
}

void GoalGate::Place(const RailField& railField, const ClearGate& shape, float gateOffset, float turnDegrees){
    shape_ = shape;
    placed_ = false;
    if ( !railField.HasGoal() ) { return; }
    const auto& rails = railField.GetRails();
    rail_ = railField.GetGoalRail();
    const SplineRail& rail = rails[rail_];
    const float length = rail.GetLength();
    if ( length <= kEndMargin * 2.0f ) { return; }
    railDistance_ = std::clamp(railField.GetGoalDistance() + gateOffset, kEndMargin, length - kEndMargin);
    position_ = rail.GetPositionByDistance(railDistance_);
    const Vector3 tangent = rail.GetTangentByDistance(railDistance_);
    forward_ = VectorMath::NormalizeSafe({ tangent.x, 0.0f, tangent.z }, { 1.0f, 0.0f, 0.0f });

    // 遊んでいる時のカメラは、世界の -Z の側から見ている。入口の面（-front）をそちらへ回す＝表をその反対へ回す
    Vector3 cameraSide { forward_.z, 0.0f, -forward_.x };
    if ( cameraSide.z > 0.0f ) { cameraSide = cameraSide * -1.0f; }
    const float turn = Math::ToRadians(turnDegrees);
    front_ = VectorMath::NormalizeSafe(forward_ * std::cos(turn) - cameraSide * std::sin(turn), forward_);
    yaw_ = std::atan2(front_.x, front_.z);
    placed_ = true;
    Close();
}

Vector3 GoalGate::Right() const{
    // モデルの +x（門の表から見て左）の、反対
    return Vector3 { -front_.z, 0.0f, front_.x };
}

Vector3 GoalGate::ToWorld(const Vector3& local) const{
    return position_ + MatrixMath::Transforms(local, MatrixMath::MakeRotateY(yaw_));
}

Vector3 GoalGate::EggCenter() const{
    return ToWorld({ 0.0f, shape_.eggHinge.y - kEggRadius * 0.3f, 0.0f });
}

Vector3 GoalGate::TapeCenter() const{
    return ToWorld({ 0.0f, shape_.tapePivot.y, 0.0f });
}

void GoalGate::Close(){
    opening_ = false;
    openTime_ = 0.0f;
    ApplyPose();
}

void GoalGate::Open(){
    if ( opening_ ) { return; }
    opening_ = true;
    openTime_ = 0.0f;
}

bool GoalGate::CheckPass(const Vector3& previous, const Vector3& current, float& outDirection) const{
    if ( !placed_ ) { return false; }
    const Vector3 move = current - previous;
    if ( VectorMath::LengthSq(move) > kMaxStep * kMaxStep ) { return false; }
    // 門の面（レールに垂直）からの、符号付きの距離（レールの進む向きが＋）
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

void GoalGate::Update(float deltaTime){
    if ( !placed_ ) { return; }
    if ( opening_ ) { openTime_ += deltaTime; }
    ApplyPose();
    for ( Obj3d* piece : { gate_.get(), eggBottom_.get(), eggTop_.get(), tapeLeft_.get(), tapeRight_.get() } ) {
        if ( piece ) { piece->Update(); }
    }
}

void GoalGate::PlacePiece(Obj3d* piece, const Vector3& pivot, const Matrix4x4& local, float lift){
    if ( !piece ) { return; }
    using namespace MatrixMath;
    // モデルの中で pivot を動かさずに local だけ回す → その後、門の向きへ
    const Matrix4x4 facing = MakeRotateY(yaw_);
    const Vector3 turned = Transforms(pivot, local);
    const Vector3 offset { pivot.x - turned.x, pivot.y - turned.y + lift, pivot.z - turned.z };
    piece->SetTranslation(position_ + Transforms(offset, facing));
    piece->SetRotation(ToEulerXYZ(Multiply(local, facing)));
}

// 卵のふた：入口の側のちょうつがいを軸に、後ろへ倒れる（行き過ぎて戻る）。開き始めにぽんと跳ねる。
// テープ：真ん中で切れて、左右の柱の根元を軸に垂れる（弾んで止まる）
void GoalGate::ApplyPose(){
    using namespace MatrixMath;
    const float progress = ( shape_.openTime > 0.0f ) ? Easing::Clamp01(openTime_ / shape_.openTime) : 1.0f;
    const float eggAngle = opening_ ? shape_.eggAngle * Easing::EaseOutBack(progress) : 0.0f;
    const float hopProgress = Easing::Clamp01(progress / kHopShare);
    const float hop = opening_ ? shape_.hop * std::sin(hopProgress * Math::kPi) * ( 1.0f - progress * 0.5f ) : 0.0f;
    const float tapeProgress = Easing::Clamp01(progress / kTapeShare);
    const float tapeAngle = opening_ ? shape_.tapeAngle * Easing::EaseOutBounce(tapeProgress) : 0.0f;

    const Matrix4x4 identity = MakeIdentity4x4();
    PlacePiece(gate_.get(), {}, identity, 0.0f);
    PlacePiece(eggBottom_.get(), {}, identity, 0.0f);
    // ちょうつがいは入口の側（モデルの -z）。モデルの X まわりの負の向きで、上が -z へ倒れる
    const Vector3 eggHinge { shape_.eggHinge.x, shape_.eggHinge.y, -shape_.eggHinge.z };
    PlacePiece(eggTop_.get(), eggHinge, MakeRotateX(-eggAngle), hop);
    // モデルの +x の柱から真ん中へ伸びるテープは、Z まわりの＋で先が下がる。-x の側は逆
    PlacePiece(tapeLeft_.get(), { shape_.tapePivot.x, shape_.tapePivot.y, 0.0f }, MakeRotateZ(tapeAngle), 0.0f);
    PlacePiece(tapeRight_.get(), { -shape_.tapePivot.x, shape_.tapePivot.y, 0.0f }, MakeRotateZ(-tapeAngle), 0.0f);
}

void GoalGate::Draw(){
    if ( !placed_ ) { return; }
    for ( Obj3d* piece : { gate_.get(), eggBottom_.get(), eggTop_.get(), tapeLeft_.get(), tapeRight_.get() } ) {
        if ( piece ) { piece->Draw(); }
    }
}
