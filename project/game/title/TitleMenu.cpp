#include "game/title/TitleMenu.h"
#include "game/title/TitleAssets.h"
#include "game/title/TitleLayout.h"

#include "engine/3d/model/Model.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/utils/Easing.h"

#include <algorithm>
#include <cmath>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kFacingYaw     = kPi;    // モデルの正面(+Z)をカメラ（-Z側）へ向ける
    constexpr float kAppearTime    = 0.35f;  // 1つが出てくる時間(秒)
    constexpr float kAppearStagger = 0.12f;  // 隣が出るまでのずれ(秒)
    constexpr float kTargetScale   = TitleLayout::kScale; // 的の大きさ（ジオラマ全体の倍率に合わせる）
    constexpr float kSelectedScale = 1.16f;  // 選んでいる的の大きさ
    constexpr float kDimColor      = 0.72f;  // 選んでいない的の明るさ
}

TitleMenu::TitleMenu() = default;
TitleMenu::~TitleMenu() = default;

void TitleMenu::Initialize(){
    for ( int i = 0; i < Item_Count; ++i ) {
        targets_[i].object = Obj3d::Create(TitleAssets::kMenuModels[i]);
        targets_[i].position = { TitleLayout::kMenuSpacing * ( float ) ( i - 1 ), TitleLayout::kGroundTop, TitleLayout::kMenuZ };
        targets_[i].appearDelay = kAppearStagger * ( float ) i;
    }
    selected_ = -1;
    Reset();
}

void TitleMenu::Finalize(){
    for ( int i = 0; i < Item_Count; ++i ) {
        targets_[i].object.reset();
    }
}

void TitleMenu::Reset(){
    for ( Target& target : targets_ ) {
        target.appearTime = -1.0f;
        target.hitTime = -1.0f;
        target.refuseTime = -1.0f;
        target.focus = 0.0f;
    }
    shown_ = false;
    time_ = 0.0f;
}

void TitleMenu::Show(){
    if ( shown_ ) { return; }
    shown_ = true;
    time_ = 0.0f;
}

void TitleMenu::ShowImmediately(){
    shown_ = true;
    time_ = 10.0f;
    for ( Target& target : targets_ ) { target.appearTime = kAppearTime; }
}

bool TitleMenu::IsReady() const{
    if ( !shown_ ) { return false; }
    for ( const Target& target : targets_ ) {
        if ( target.appearTime < kAppearTime ) { return false; }
    }
    return true;
}

void TitleMenu::SetSelected(int item){
    selected_ = ( item >= 0 && item < Item_Count ) ? item : -1;
}

// worldPosition にいちばん近い的（水平距離が maxDistance 以内。無ければ -1）
int TitleMenu::FindNearest(const Vector3& worldPosition, float maxDistance) const{
    int nearest = -1;
    float nearestDistance = maxDistance;
    for ( int i = 0; i < Item_Count; ++i ) {
        const float dx = targets_[i].position.x - worldPosition.x;
        const float dz = targets_[i].position.z - worldPosition.z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        if ( distance <= nearestDistance ) { nearestDistance = distance; nearest = i; }
    }
    return nearest;
}

Vector3 TitleMenu::GetTargetCenter(int item) const{
    const Vector3& base = targets_[item].position;
    return { base.x, base.y + TitleLayout::kMenuFaceCenterHeight, base.z };
}

void TitleMenu::Hit(int item){
    targets_[item].hitTime = 0.0f;
}

void TitleMenu::Refuse(int item){
    targets_[item].refuseTime = 0.0f;
}

void TitleMenu::Update(float deltaTime){
    if ( shown_ ) { time_ += deltaTime; }

    for ( int i = 0; i < Item_Count; ++i ) {
        Target& target = targets_[i];
        if ( !target.object ) { continue; }

        // 出てくる：地面から、少し行き過ぎて戻る大きさで伸びる
        if ( shown_ && time_ >= target.appearDelay ) {
            target.appearTime = ( std::max )( target.appearTime, 0.0f ) + deltaTime;
        }
        const float appear = ( target.appearTime < 0.0f )
            ? 0.0f : Easing::EaseOutBack(target.appearTime / kAppearTime);

        // 選ばれ具合をなめらかに追従させる
        const float wanted = ( i == selected_ && IsReady() ) ? 1.0f : 0.0f;
        target.focus += ( wanted - target.focus ) * ( std::min )( deltaTime * 12.0f, 1.0f );

        // 選んでいる的：大きくなって、ゆっくり弾む
        const float pulse = 1.0f + 0.04f * std::sin(time_ * 5.0f);
        const float size = kTargetScale * appear * Easing::Lerp(1.0f, kSelectedScale * pulse, target.focus);

        Vector3 rotation { 0.0f, kFacingYaw, 0.0f };
        // 地面に刺した棒の先なので、いつもほんの少しゆらゆらしている（的ごとにずらす）
        rotation.z = 0.022f * appear * std::sin(time_ * 1.4f + ( float ) i * 1.9f);
        // ベロが当たった：奥へ倒れかけて、揺れながら戻る
        if ( target.hitTime >= 0.0f ) {
            target.hitTime += deltaTime;
            rotation.x = -0.45f * std::exp(-4.5f * target.hitTime) * std::sin(target.hitTime * 16.0f);
            if ( target.hitTime > 1.5f ) { target.hitTime = -1.0f; }
        }
        // 断る：横に小刻みに震える
        if ( target.refuseTime >= 0.0f ) {
            target.refuseTime += deltaTime;
            rotation.z += 0.14f * std::exp(-6.0f * target.refuseTime) * std::sin(target.refuseTime * 40.0f);
            if ( target.refuseTime > 1.0f ) { target.refuseTime = -1.0f; }
        }

        target.object->SetTranslation(target.position);
        target.object->SetRotation(rotation);
        target.object->SetScale({ size, size, size });
        target.object->Update();

        // 選んでいない的は少し暗くする（モデルは的ごとに別なので、色も別々に持てる）
        if ( Model* model = target.object->GetModel() ) {
            const float brightness = Easing::Lerp(kDimColor, 1.0f, target.focus);
            model->SetColor({ brightness, brightness, brightness, 1.0f });
        }
    }
}

void TitleMenu::Draw(){
    for ( Target& target : targets_ ) {
        if ( target.object && target.appearTime >= 0.0f ) { target.object->Draw(); }
    }
}
