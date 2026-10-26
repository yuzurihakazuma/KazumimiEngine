#include "game/hud/HeartRing.h"

#include "engine/2d/Sprite.h"
#include "engine/base/WindowProc.h"
#include "engine/camera/Camera.h"
#include "engine/graphics/TextureManager.h"
#include "engine/math/Matrix4x4.h"
#include "engine/utils/Easing.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr const char* kTexture = "resources/gameover/heart.png";
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kShowSeconds = 1.8f;   // 出しておく時間
    constexpr float kFadeSeconds = 0.3f;   // 消えるまで
    constexpr float kOpenTime = 0.3f;      // 輪がぱっと広がる時間
    constexpr float kBreakTime = 0.7f;     // はじけて落ちる時間
    constexpr float kRingScale = 0.75f;    // 輪の半径（画面に映った背の高さに対して）
    constexpr float kHeartScale = 0.42f;   // ハートの大きさ（となりとの間に対して）
    constexpr float kLowShare = 0.25f;     // これより少なくなったら、ずっと出して脈打つ
}

HeartRing::HeartRing() = default;
HeartRing::~HeartRing() = default;

void HeartRing::Initialize(int maxHearts){
    max_ = ( std::max )( 1, maxHearts );
    count_ = max_;
    const uint32_t texture = TextureManager::GetInstance()->Load(kTexture).srvIndex;
    hearts_.clear();
    for ( int i = 0; i < max_; ++i ) {
        Heart heart;
        heart.sprite = Sprite::Create(texture, { 0.0f, 0.0f });
        heart.driftX = ( i % 2 == 0 ) ? 1.0f : -1.0f;
        hearts_.push_back(std::move(heart));
    }
    showTime_ = -1.0f;
}

void HeartRing::Finalize(){
    hearts_.clear();
}

void HeartRing::Show(){
    // 隠れていたら輪を広げ直す。出ている間は時間だけ延ばす
    showTime_ = ( showTime_ < 0.0f || showTime_ > kShowSeconds ) ? 0.0f : ( std::min )( showTime_, kOpenTime );
}

bool HeartRing::Damage(int amount){
    if ( count_ <= 0 ) { return true; }
    Show();
    for ( int i = 0; i < amount && count_ > 0; ++i ) {
        --count_;
        hearts_[count_].broken = true;
        hearts_[count_].breakTime = -0.06f * ( float ) i; // 輪の終わりの方から、少しずつずらしてはじける
    }
    return count_ <= 0;
}

void HeartRing::Refill(){
    count_ = max_;
    for ( Heart& heart : hearts_ ) { heart.broken = false; heart.breakTime = 0.0f; }
    showTime_ = -1.0f;
    Show();
}

void HeartRing::HideNow(){
    showTime_ = -1.0f;
}

void HeartRing::Update(float deltaTime, const Vector3& feet, float height, const Camera& camera){
    pulse_ += deltaTime;
    const bool low = count_ > 0 && ( float ) count_ <= ( float ) max_ * kLowShare;
    if ( low && showTime_ > kShowSeconds - kFadeSeconds ) { showTime_ = kShowSeconds - kFadeSeconds - 0.01f; } // あぶない時は出しっぱなし
    if ( showTime_ >= 0.0f ) { showTime_ += deltaTime; }
    for ( Heart& heart : hearts_ ) {
        if ( heart.broken ) { heart.breakTime += deltaTime; }
    }
    // 足元と頭を画面へ写し、その間の真ん中を輪の中心に、背の高さから輪の大きさを決める
    Vector2 feetNdc, headNdc;
    const Matrix4x4& viewProjection = camera.GetViewProjectionMatrix();
    valid_ = MatrixMath::WorldToNdc(feet, viewProjection, feetNdc) && MatrixMath::WorldToNdc({ feet.x, feet.y + height, feet.z }, viewProjection, headNdc);
    if ( valid_ ) {
        const float width = ( float ) WindowProc::GetInstance()->GetClientWidth();
        const float screenHeight = ( float ) WindowProc::GetInstance()->GetClientHeight();
        auto toScreen = [&](const Vector2& ndc){ return Vector2 { ( ndc.x * 0.5f + 0.5f ) * width, ( 1.0f - ( ndc.y * 0.5f + 0.5f ) ) * screenHeight }; };
        const Vector2 a = toScreen(feetNdc), b = toScreen(headNdc);
        center_ = { ( a.x + b.x ) * 0.5f, ( a.y + b.y ) * 0.5f };
        const float projected = std::sqrt(( a.x - b.x ) * ( a.x - b.x ) + ( a.y - b.y ) * ( a.y - b.y ));
        radius_ = std::clamp(projected * kRingScale, 50.0f * screenHeight / 1080.0f, 260.0f * screenHeight / 1080.0f);
    }
}

void HeartRing::Draw(){
    if ( showTime_ < 0.0f || showTime_ > kShowSeconds || !valid_ || hearts_.empty() ) { return; }
    const float fade = 1.0f - Easing::Clamp01(( showTime_ - ( kShowSeconds - kFadeSeconds ) ) / kFadeSeconds);
    const float open = Easing::EaseOutBack(Easing::Clamp01(showTime_ / kOpenTime)); // 輪がぱっと広がる
    const int n = ( int ) hearts_.size();
    const float spacing = 2.0f * kPi * radius_ / ( float ) n;
    const float size = ( std::min )( spacing * kHeartScale * 2.0f, radius_ * 0.45f );
    const bool low = count_ > 0 && ( float ) count_ <= ( float ) max_ * kLowShare;
    for ( int i = 0; i < n; ++i ) {
        Heart& heart = hearts_[i];
        const bool alive = i < count_;
        const bool waiting = !alive && heart.broken && heart.breakTime < 0.0f;
        const bool breaking = !alive && heart.broken && heart.breakTime >= 0.0f && heart.breakTime < kBreakTime;
        if ( !alive && !waiting && !breaking ) { continue; }
        // 真上から時計回りに並べる（輪がくるっと回りながら広がる）
        const float angle = -kPi * 0.5f + 2.0f * kPi * ( float ) i / ( float ) n + ( 1.0f - open ) * 0.8f;
        Vector2 at { center_.x + std::cos(angle) * radius_ * open, center_.y + std::sin(angle) * radius_ * open };
        float s = size * ( 0.6f + 0.4f * Easing::Clamp01(open) );
        float alpha = fade;
        float rotation = 0.0f;
        if ( breaking ) {
            // ポンとふくらんで、外へはじけながら落ちる
            const float t = heart.breakTime / kBreakTime;
            s *= 1.0f + 0.6f * std::sin(Easing::Clamp01(t * 2.5f) * kPi);
            at.x += ( std::cos(angle) * 60.0f + heart.driftX * 30.0f ) * t;
            at.y += ( std::sin(angle) * 60.0f - 140.0f * t + 420.0f * t * t ) * t;
            rotation = heart.driftX * 2.0f * t;
            alpha *= 1.0f - t * t;
        } else if ( low ) {
            s *= 1.0f + 0.12f * std::sin(pulse_ * 9.0f); // 残り少ない：脈打つ
        }
        heart.sprite->SetPosition(at);
        heart.sprite->SetSize({ s, s });
        heart.sprite->SetRotation(rotation);
        heart.sprite->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
        heart.sprite->Update();
        heart.sprite->Draw();
    }
}
