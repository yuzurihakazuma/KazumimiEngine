#include "game/title/TitleLogoBoard.h"
#include "game/title/TitleAssets.h"

#include "engine/3d/obj/Obj3d.h"
#include "engine/math/VectorMath.h"
#include "engine/utils/Easing.h"

#include <cmath>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kFlightTime   = 0.9f;  // 飛んでいる時間(秒)
    constexpr float kFlightHeight = 2.6f * TitleLayout::kScale; // 山なりの高さ(m)
    constexpr float kBoardScale   = TitleLayout::kScale;        // 刺さった時の大きさ
    constexpr float kFlightSpins  = 1.5f;  // 飛びながら回る回数
    constexpr float kFacingYaw    = kPi;   // モデルの正面(+Z)をカメラ（-Z側）へ向ける
}

TitleLogoBoard::TitleLogoBoard() = default;
TitleLogoBoard::~TitleLogoBoard() = default;

void TitleLogoBoard::Initialize(){
    board_ = Obj3d::Create(TitleAssets::kLogoBoardModel);
    Reset();
}

void TitleLogoBoard::Finalize(){
    board_.reset();
}

void TitleLogoBoard::Reset(){
    state_ = State::Hidden;
    time_ = 0.0f;
    landedPending_ = false;
}

void TitleLogoBoard::Launch(const Vector3& from){
    launchPosition_ = from;
    state_ = State::Flying;
    time_ = 0.0f;
}

void TitleLogoBoard::PlaceLanded(){
    state_ = State::Landed;
    time_ = 10.0f; // 揺れが収まった後の状態
    landedPending_ = false;
}

bool TitleLogoBoard::ConsumeLanded(){
    const bool pending = landedPending_;
    landedPending_ = false;
    return pending;
}

void TitleLogoBoard::Update(float deltaTime){
    if ( !board_ || state_ == State::Hidden ) { return; }
    time_ += deltaTime;

    Vector3 position = landPosition_;
    Vector3 rotation { 0.0f, kFacingYaw, 0.0f };
    Vector3 scale { kBoardScale, kBoardScale, kBoardScale };

    if ( state_ == State::Flying ) {
        const float t = Easing::Clamp01(time_ / kFlightTime);
        // 山なりに飛ぶ。小さく出て、回りながら大きくなって、最後は真っすぐ刺さる
        position = VectorMath::Lerp(launchPosition_, landPosition_, t);
        position.y += kFlightHeight * 4.0f * t * ( 1.0f - t );
        rotation.z = ( 1.0f - Easing::EaseOutCubic(t) ) * kFlightSpins * kPi * 2.0f;
        const float size = kBoardScale * Easing::Lerp(0.3f, 1.0f, Easing::EaseOutQuad(t));
        scale = { size, size, size };
        if ( t >= 1.0f ) {
            state_ = State::Landed;
            time_ = 0.0f;
            landedPending_ = true;
        }
    } else {
        // 刺さった直後：左右にぐらぐら揺れて、縦に少しつぶれてから戻る（だんだん収まる）
        const float wobble = std::exp(-5.0f * time_);
        rotation.z = 0.16f * wobble * std::sin(time_ * 17.0f);
        scale.y = kBoardScale * ( 1.0f - 0.14f * std::exp(-8.0f * time_) * std::cos(time_ * 20.0f) );
    }

    board_->SetTranslation(position);
    board_->SetRotation(rotation);
    board_->SetScale(scale);
    board_->Update();
}

void TitleLogoBoard::Draw(){
    if ( board_ && state_ != State::Hidden ) { board_->Draw(); }
}
