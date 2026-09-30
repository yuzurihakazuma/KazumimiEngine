#include "game/title/TitlePaperBits.h"
#include "game/title/TitleAssets.h"
#include "game/title/TitleLayout.h"

#include "engine/3d/obj/Obj3d.h"
#include "engine/graphics/PipelineType.h"
#include "engine/utils/Easing.h"
#include "engine/utils/Random.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr int   kPoolSize = 48;          // 用意する枚数（同時に出せる上限）
    constexpr int   kDriftCount = 9;         // ただよう紙の枚数（控えめ）
    constexpr float kGravity = 5.5f;         // 紙なので軽く落ちる(m/s^2)
    constexpr float kDrag = 2.2f;            // 空気でだんだん遅くなる
    constexpr float kFallSpeedLimit = 1.6f;  // ひらひら落ちる時の速さの上限(m/s)
    constexpr float kFadeTime = 0.35f;       // 消える直前に縮む時間(秒)

    // ただよう紙が出る範囲（カメラに映る空間の上の方）
    constexpr float kDriftHalfWidth = 7.5f * TitleLayout::kScale;
    constexpr float kDriftTop       = 5.2f * TitleLayout::kScale;
    constexpr float kDriftNear      = -2.5f * TitleLayout::kScale;
    constexpr float kDriftFar       = 3.0f * TitleLayout::kScale;
}

TitlePaperBits::TitlePaperBits() = default;
TitlePaperBits::~TitlePaperBits() = default;

void TitlePaperBits::Initialize(){
    bits_.clear();
    bits_.resize(kPoolSize);
    for ( int i = 0; i < kPoolSize; ++i ) {
        // 色は紙のモデルごとに違う。順番に割り当てて、どの演出でも色が混ざるようにする
        bits_[i].object = Obj3d::Create(TitleAssets::kPaperBitModels[i % TitleAssets::kPaperBitModelCount]);
        if ( bits_[i].object ) {
            bits_[i].object->SetPipelineType(PipelineType::Object3D_CullNone); // 裏返っても見える
        }
    }
    ambientTimer_ = 0.0f;
    // 最初から数枚ただよっている状態にする（始まってから降ってくるのを待たせない）
    if ( ambient_ ) {
        for ( int i = 0; i < kDriftCount; ++i ) {
            if ( Bit* bit = FindFree() ) { SpawnDrifting(*bit, true); }
        }
    }
}

void TitlePaperBits::Finalize(){
    bits_.clear();
}

void TitlePaperBits::Clear(){
    for ( Bit& bit : bits_ ) { bit.alive = false; }
}

TitlePaperBits::Bit* TitlePaperBits::FindFree(){
    for ( Bit& bit : bits_ ) {
        if ( !bit.alive ) { return &bit; }
    }
    return nullptr;
}

void TitlePaperBits::Burst(const Vector3& position, int count, float spread, float lift, float size){
    for ( int i = 0; i < count; ++i ) {
        Bit* bit = FindFree();
        if ( !bit ) { return; }
        const float angle = Random::Float(0.0f, Easing::kPi * 2.0f);
        const float speed = Random::Float(0.35f, 1.0f) * spread;
        bit->alive = true;
        bit->drifting = false;
        bit->position = position;
        bit->velocity = { std::cos(angle) * speed, Random::Float(0.6f, 1.0f) * lift, std::sin(angle) * speed };
        bit->rotation = { Random::Float(0.0f, 6.28f), Random::Float(0.0f, 6.28f), Random::Float(0.0f, 6.28f) };
        bit->spin = { Random::Float(-7.0f, 7.0f), Random::Float(-7.0f, 7.0f), Random::Float(-7.0f, 7.0f) };
        bit->size = size * Random::Float(0.7f, 1.2f);
        bit->age = 0.0f;
        bit->life = Random::Float(1.1f, 1.8f);
        bit->swayPhase = Random::Float(0.0f, 6.28f);
    }
}

// ただよう紙を1枚出す。anywhere=true は空間のどこか（最初の配置用）、false は上の端から
void TitlePaperBits::SpawnDrifting(Bit& bit, bool anywhere){
    bit.alive = true;
    bit.drifting = true;
    bit.position = {
        Random::Float(-kDriftHalfWidth, kDriftHalfWidth),
        anywhere ? Random::Float(TitleLayout::kGroundTop + 1.0f, kDriftTop) : kDriftTop,
        Random::Float(kDriftNear, kDriftFar) };
    bit.velocity = { Random::Float(0.25f, 0.6f), -Random::Float(0.35f, 0.6f), 0.0f }; // 右下へゆっくり
    bit.rotation = { Random::Float(0.0f, 6.28f), Random::Float(0.0f, 6.28f), Random::Float(0.0f, 6.28f) };
    bit.spin = { Random::Float(-1.5f, 1.5f), Random::Float(-2.0f, 2.0f), Random::Float(-1.5f, 1.5f) };
    bit.size = Random::Float(0.2f, 0.32f);
    bit.age = 0.0f;
    bit.life = 60.0f; // 地面に着くまで（下で消す）
    bit.swayPhase = Random::Float(0.0f, 6.28f);
}

void TitlePaperBits::Update(float deltaTime){
    // ただよう紙：減った分を少しずつ足す
    if ( ambient_ ) {
        int drifting = 0;
        for ( const Bit& bit : bits_ ) { if ( bit.alive && bit.drifting ) { ++drifting; } }
        ambientTimer_ += deltaTime;
        if ( drifting < kDriftCount && ambientTimer_ >= 0.6f ) {
            ambientTimer_ = 0.0f;
            if ( Bit* bit = FindFree() ) { SpawnDrifting(*bit, false); }
        }
    }

    for ( Bit& bit : bits_ ) {
        if ( !bit.alive || !bit.object ) { continue; }
        bit.age += deltaTime;

        if ( bit.drifting ) {
            // 左右にゆれながら、決まった速さで降りる
            bit.position.x += ( bit.velocity.x + 0.5f * std::sin(bit.age * 1.3f + bit.swayPhase) ) * deltaTime;
            bit.position.y += bit.velocity.y * deltaTime;
            if ( !ambient_ || bit.position.y < TitleLayout::kGroundTop + 0.05f ) {
                // 地面に着いた（または OFF にされた）：残りの時間を短くして、縮んで消える
                bit.life = ( std::min )( bit.life, bit.age + kFadeTime );
            }
        } else {
            // 舞い上がってから、空気で遅くなって、ひらひら落ちる
            bit.velocity.x -= bit.velocity.x * kDrag * deltaTime;
            bit.velocity.z -= bit.velocity.z * kDrag * deltaTime;
            bit.velocity.y = ( std::max )( bit.velocity.y - kGravity * deltaTime, -kFallSpeedLimit );
            bit.position += bit.velocity * deltaTime;
            bit.position.x += 0.4f * std::sin(bit.age * 6.0f + bit.swayPhase) * deltaTime;
            if ( bit.position.y < TitleLayout::kGroundTop + 0.03f ) {
                bit.position.y = TitleLayout::kGroundTop + 0.03f; // 地面に落ちたら止まる
                bit.velocity = { 0.0f, 0.0f, 0.0f };
                bit.spin = { 0.0f, 0.0f, 0.0f };
            }
        }
        bit.rotation += bit.spin * deltaTime;

        if ( bit.age >= bit.life ) { bit.alive = false; continue; }
        const float fade = Easing::Clamp01(( bit.life - bit.age ) / kFadeTime); // 最後は縮んで消える
        const float size = bit.size * fade;

        bit.object->SetTranslation(bit.position);
        bit.object->SetRotation(bit.rotation);
        bit.object->SetScale({ size, size, size });
        bit.object->Update();
    }
}

void TitlePaperBits::Draw(){
    for ( Bit& bit : bits_ ) {
        if ( bit.alive && bit.object ) { bit.object->Draw(); }
    }
}
