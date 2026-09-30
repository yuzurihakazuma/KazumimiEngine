#include "game/title/TitleDino.h"

#include "game/player/Player.h"
#include "engine/3d/obj/SkinnedObj3d.h"
#include "engine/utils/Easing.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kAppearTime = 0.35f;       // ぴょこっと現れる時間(秒)
    constexpr float kWalkBaseSpeed = 5.0f;     // この速さ(m/s)で歩くクリップを等速再生する（本編と同じ）

    // ベロ：クリップ(約1.33秒)を早回しして約0.5秒の動きにする（本編と同じ速さ）
    constexpr float kTonguePlaySpeed = 2.8f;
    constexpr float kTongueTime      = 0.5f;
    constexpr float kTongueHitTime   = 0.2f;   // ベロが伸び切る頃
    constexpr float kTongueBaseLength = 1.2f;  // ベロが伸びる長さ(m)
    constexpr float kMouthOffset     = 0.35f;  // 体の中心から口までの距離(m)

    // 投げ：Throw クリップの溜めから再生し、リリース(約0.8秒)で手を離す
    constexpr float kThrowPlaySpeed   = 1.6f;
    constexpr float kThrowStartTime   = 0.45f; // クリップ内の再生開始位置(秒)
    constexpr float kThrowReleaseTime = 0.22f; // 手を離すまでの実時間(秒)
    constexpr float kThrowTime        = 0.7f;  // 投げ終わるまで
}

TitleDino::TitleDino() = default;
TitleDino::~TitleDino() = default;

void TitleDino::Initialize(uint32_t envMapSrv){
    model_ = SkinnedObj3d::Create("player", "resources/player", "player.gltf");
    if ( model_ ) {
        model_->SetEnvironmentMap(envMapSrv);
        model_->LoadClips("resources/player", "player.gltf");
        model_->SetClip("Idle", true);
    }
    visible_ = false;
    prevPositionValid_ = false;
}

void TitleDino::Finalize(){
    model_.reset();
}

void TitleDino::Appear(){
    visible_ = true;
    appearTime_ = 0.0f;
    prevPositionValid_ = false;
    CancelAction();
}

void TitleDino::ShowImmediately(){
    visible_ = true;
    appearTime_ = kAppearTime;
    prevPositionValid_ = false;
    CancelAction();
}

void TitleDino::CancelAction(){
    action_ = Action::None;
    tongueHitPending_ = false;
    throwReleasePending_ = false;
    if ( model_ ) { model_->ClearBoneScaleOverride(); }
}

void TitleDino::ShootTongue(float distance){
    if ( action_ != Action::None || !model_ ) { return; }
    // 口は体の中心より少し前にあるので、その分を引いた距離だけ伸ばす
    tongueStretch_ = ( std::max )( distance - kMouthOffset, 0.1f ) / kTongueBaseLength;
    action_ = Action::Tongue;
    actionTime_ = 0.0f;
    tongueHitFired_ = false;
    model_->SetClip("TongueOut", false);
    model_->SetAnimationTime(0.0f);
    model_->SetPlaybackSpeed(kTonguePlaySpeed);
}

void TitleDino::StartThrow(){
    if ( action_ != Action::None || !model_ ) { return; }
    action_ = Action::Throw;
    actionTime_ = 0.0f;
    throwReleaseFired_ = false;
    model_->SetClip("Throw", false);
    model_->SetAnimationTime(kThrowStartTime);
    model_->SetPlaybackSpeed(kThrowPlaySpeed);
}

bool TitleDino::ConsumeTongueHit(){
    const bool pending = tongueHitPending_;
    tongueHitPending_ = false;
    return pending;
}

bool TitleDino::ConsumeThrowRelease(){
    const bool pending = throwReleasePending_;
    throwReleasePending_ = false;
    return pending;
}

Vector3 TitleDino::GetHandPosition() const{
    Vector3 hand {};
    if ( model_ && model_->GetJointWorldPosition("Item", hand) ) { return hand; }
    return { position_.x, position_.y + 1.2f, position_.z };
}

// 歩く／止まる／ジャンプ／ふんばり（PlayerAvatar と同じ選び方）
void TitleDino::SelectMoveClip(const Player& player, float deltaTime){
    // 実際に動いた速さ（見た目の位置の差分）でクリップと再生速度を決める
    float speed = 0.0f;
    if ( prevPositionValid_ && deltaTime > 0.0f ) {
        const float dx = position_.x - prevPosition_.x;
        const float dz = position_.z - prevPosition_.z;
        speed = std::sqrt(dx * dx + dz * dz) / deltaTime;
    }
    const float speedRatio = std::clamp(speed / kWalkBaseSpeed, 0.0f, 1.8f);

    model_->ClearBoneScaleOverride();
    if ( !player.IsGrounded() ) {
        model_->SetClip("Walk", true);
        if ( player.IsFluttering() ) {
            model_->SetPlaybackSpeed(3.2f);   // ふんばり：足を高速でバタバタ
        } else {
            model_->SetPlaybackSpeed(0.0f);   // ジャンプ中：足を伸ばしたポーズで止める
            model_->SetAnimationTime(0.25f);
        }
    } else if ( speedRatio > 0.05f ) {
        model_->SetClip("Walk", true);
        model_->SetPlaybackSpeed(( std::max )( speedRatio, 0.4f ));
    } else {
        model_->SetClip("Idle", true);
        model_->SetPlaybackSpeed(1.0f);
    }
}

void TitleDino::Update(const Player& player, float deltaTime){
    if ( !model_ || !visible_ ) { return; }
    appearTime_ += deltaTime;
    position_ = player.GetPosition();

    switch ( action_ ) {
    case Action::None:
        SelectMoveClip(player, deltaTime);
        break;
    case Action::Tongue:
        actionTime_ += deltaTime;
        // ベロの長さを的までの距離に合わせる（Tongue ボーンの伸びを上書き）
        model_->SetBoneScaleOverride("Tongue", { 1.0f, tongueStretch_, 1.0f });
        if ( !tongueHitFired_ && actionTime_ >= kTongueHitTime ) {
            tongueHitFired_ = true;
            tongueHitPending_ = true;
        }
        if ( actionTime_ >= kTongueTime ) { action_ = Action::None; }
        break;
    case Action::Throw:
        actionTime_ += deltaTime;
        if ( !throwReleaseFired_ && actionTime_ >= kThrowReleaseTime ) {
            throwReleaseFired_ = true;
            throwReleasePending_ = true;
        }
        if ( actionTime_ >= kThrowTime ) { action_ = Action::None; }
        break;
    }
    prevPosition_ = position_;
    prevPositionValid_ = true;

    // 現れる時は、少し行き過ぎて戻る大きさで伸びる
    const float size = Easing::EaseOutBack(appearTime_ / kAppearTime);
    model_->SetScale({ size, size, size });
    model_->SetTranslation(position_);
    model_->SetRotation({ 0.0f, player.GetRotation().y, 0.0f });
    model_->Update();
}

void TitleDino::Draw(){
    if ( model_ && visible_ ) { model_->Draw(); }
}
