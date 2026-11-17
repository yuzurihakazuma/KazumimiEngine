#include "game/clear/GoalProps.h"

#include "engine/3d/obj/Obj3d.h"
#include "engine/audio/AudioManager.h"
#include "engine/math/MathUtil.h"
#include "engine/math/Matrix4x4.h"
#include "engine/math/VectorMath.h"
#include "engine/utils/Easing.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
    constexpr const char* kLetterModel = "goalLetter";   // ＋番号（goal_letter_0〜3.gltf）
    constexpr const char* kStarModel = "goalStar";       // ＋番号（goal_star_0〜2.gltf）
    constexpr const char* kStringModel = "goalString";   // 長さ 1m の糸（原点＝上のはし）
    constexpr int   kStarColors = 3;
    constexpr float kDepthStep = 0.03f;        // 小道具どうしが同じ奥行きで重ならないよう、1 つごとに手前へずらす(m)
    constexpr float kAttachInset = 0.25f;      // 糸をつなぐ所：アーチの外のふちから、これだけ内側
    constexpr float kMinString = 0.12f;        // 糸のいちばん短い長さ(m)
    constexpr float kSwingStart = 0.6f;        // 文字：下りる時間のこの割合から、糸の揺れが始まる
    constexpr float kSwingDecay = 3.0f;        // 糸の揺れがおさまる速さ
    constexpr float kSwingRate = 1.4f;         // 糸の揺れの速さ(回/秒)
    constexpr float kSwayRate = 0.35f;         // その後の、ゆっくりした揺れの速さ(回/秒)
    constexpr float kStarWobble = 0.05f;       // 星：出た後の、小さな揺れ(rad)
    constexpr float kLetterVolume = 0.6f, kStarVolume = 0.4f;
}

GoalProps::GoalProps() = default;
GoalProps::~GoalProps() = default;

void GoalProps::Initialize(const ClearSettings& settings){
    settings_ = &settings;
    letters_.clear();
    for ( int i = 0; i < settings.letters.count; ++i ) {
        Piece piece;
        piece.object = Obj3d::Create(kLetterModel + std::to_string(i));
        piece.string = Obj3d::Create(kStringModel);
        letters_.push_back(std::move(piece));
    }
    stars_.clear();
    for ( int i = 0; i < settings.stars.perSide * 2; ++i ) {
        Piece piece;
        piece.object = Obj3d::Create(kStarModel + std::to_string(i % kStarColors));
        stars_.push_back(std::move(piece));
    }
    AudioManager* audio = AudioManager::GetInstance();
    audio->LoadWave(settings.sounds.letter);
    audio->LoadWave(settings.sounds.star);
}

void GoalProps::Begin(const Vector3& origin, const Vector3& right, const Vector3& front, const Vector3& toCamera){
    origin_ = origin;
    right_ = right;
    front_ = front;
    faceYaw_ = std::atan2(toCamera.x, toCamera.z); // モデルの表（+Z）をカメラの方へ
    visible_ = true;
    for ( Piece& piece : letters_ ) { piece.soundPlayed = false; }
    for ( Piece& piece : stars_ ) { piece.soundPlayed = false; }
    Update(0.0f);
}

void GoalProps::Hide(){
    visible_ = false;
}

Vector3 GoalProps::ToWorld(const Vector3& stagePoint) const{
    return origin_ + right_ * stagePoint.x + Vector3 { 0.0f, stagePoint.y, 0.0f } + front_ * stagePoint.z;
}

// 表（+Z）がカメラを向いている時、モデルの Z まわりの＋は画面では右回りに見える。画面の左回りを＋にするので逆に回す
Vector3 GoalProps::FacingRotation(float angle) const{
    using namespace MatrixMath;
    return ToEulerXYZ(Multiply(MakeRotateZ(-angle), MakeRotateY(faceYaw_)));
}

float GoalProps::ArchHeightAt(float across) const{
    const ClearGate& gate = settings_->gate;
    const float radius = ( std::max )( gate.archOuterRadius - kAttachInset, 0.1f );
    const float x = ( std::min )( std::abs(across), radius );
    return gate.archCenterHeight + std::sqrt(( std::max )( radius * radius - x * x, 0.0f ));
}

void GoalProps::Update(float elapsed){
    if ( !visible_ || !settings_ ) { return; }
    const ClearLetters& letters = settings_->letters;
    const ClearStars& stars = settings_->stars;
    const ClearSounds& sounds = settings_->sounds;
    AudioManager* audio = AudioManager::GetInstance();

    // --- 切り文字：アーチにつないだ糸が伸びて下がる（行き過ぎて戻る）→ 糸で揺れる ---
    const int letterCount = static_cast<int>( letters_.size() );
    for ( int i = 0; i < letterCount; ++i ) {
        Piece& piece = letters_[i];
        const float time = elapsed - ( letters.start + letters.interval * static_cast<float>( i ) );
        const float drop = ( letters.dropTime > 0.0f ) ? Easing::Clamp01(time / letters.dropTime) : 1.0f;
        if ( time >= 0.0f && !piece.soundPlayed ) {
            piece.soundPlayed = true;
            audio->PlayWave(sounds.letter, false, kLetterVolume);
        }
        const float across = ( static_cast<float>( i ) - static_cast<float>( letterCount - 1 ) * 0.5f ) * letters.spacing;
        const float attach = ArchHeightAt(across);
        const float restTop = letters.centerHeight + letters.height * 0.5f;
        const float restLength = ( std::max )( attach - restTop, kMinString );
        // 出る前は札を見せない（アーチの中に隠れている）。下りながら、ぽんと大きくなる
        const float length = ( time >= 0.0f ) ? restLength * Easing::EaseOutBack(drop) : 0.0f;
        const float scale = ( time >= 0.0f ) ? ( std::min )( drop * 4.0f, 1.0f ) : 0.0f;
        const float swingTime = ( std::max )( time - letters.dropTime * kSwingStart, 0.0f );
        const float swing = letters.swing * std::exp(-kSwingDecay * swingTime) * std::sin(2.0f * Math::kPi * kSwingRate * swingTime);
        const float sway = letters.sway * std::sin(2.0f * Math::kPi * kSwayRate * elapsed + static_cast<float>( i ) * 1.7f);
        const float angle = ( time >= 0.0f ) ? swing + sway : 0.0f;
        // 支点＝アーチの糸をつなぐ所。札の上のはしは、そこから糸の長さだけ下（画面の中で angle だけ振れる）
        const float depth = settings_->gate.archFront + letters.front + kDepthStep * static_cast<float>( i );
        const Vector3 pivot { across, attach, depth };
        const Vector3 top { pivot.x + std::sin(angle) * length, pivot.y - std::cos(angle) * length, depth };
        const Vector3 rotation = FacingRotation(angle);
        if ( piece.object ) {
            piece.object->SetTranslation(ToWorld(top));
            piece.object->SetRotation(rotation);
            piece.object->SetScale({ ( std::max )( scale, 0.001f ), ( std::max )( scale, 0.001f ), ( std::max )( scale, 0.001f ) });
            piece.object->Update();
        }
        if ( piece.string ) {
            piece.string->SetTranslation(ToWorld(pivot));
            piece.string->SetRotation(rotation);
            piece.string->SetScale({ 1.0f, ( std::max )( length, 0.001f ), 1.0f });
            piece.string->Update();
        }
    }

    // --- 棒付きの星：門の両わきの足元から、外へ扇状に飛び出す（行き過ぎて戻る）---
    const int starCount = static_cast<int>( stars_.size() );
    for ( int i = 0; i < starCount; ++i ) {
        Piece& piece = stars_[i];
        const int slot = i / 2;                            // 内側から何本目か
        const float side = ( i % 2 == 0 ) ? -1.0f : 1.0f;  // 左・右
        const float time = elapsed - ( stars.start + stars.interval * static_cast<float>( i ) );
        const float pop = ( stars.popTime > 0.0f ) ? Easing::Clamp01(time / stars.popTime) : 1.0f;
        if ( time >= 0.0f && !piece.soundPlayed ) {
            piece.soundPlayed = true;
            audio->PlayWave(sounds.star, false, kStarVolume);
        }
        const float fan = stars.angles.empty() ? 0.6f : stars.angles[( std::min )( slot, static_cast<int>( stars.angles.size() ) - 1 )];
        const float wobble = kStarWobble * std::sin(2.0f * Math::kPi * kSwayRate * 2.0f * elapsed + static_cast<float>( i ));
        // 外へ倒す：左の星は左（左回り＝＋）、右の星は右
        const float angle = -side * ( fan + wobble );
        const float scale = ( time >= 0.0f ) ? Easing::EaseOutBack(pop) : 0.0f; // 根元から伸びて出る
        const Vector3 pivot { side * stars.pivotSide, stars.pivotHeight, stars.front + kDepthStep * static_cast<float>( i ) };
        if ( piece.object ) {
            piece.object->SetTranslation(ToWorld(pivot));
            piece.object->SetRotation(FacingRotation(angle));
            const float s = ( std::max )( scale, 0.001f );
            piece.object->SetScale({ s, s, s });
            piece.object->Update();
        }
    }
}

void GoalProps::Draw(){
    if ( !visible_ ) { return; }
    for ( Piece& piece : stars_ ) { if ( piece.object ) { piece.object->Draw(); } }
    for ( Piece& piece : letters_ ) {
        if ( piece.string ) { piece.string->Draw(); }
        if ( piece.object ) { piece.object->Draw(); }
    }
}
