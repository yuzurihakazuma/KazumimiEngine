#include "game/flip/WorldFlip.h"

#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/audio/AudioManager.h"
#include "engine/base/Input.h"
#include "engine/base/TimeManager.h"
#include "engine/graphics/DebugDraw.h"
#include "engine/graphics/PipelineType.h"
#include "engine/rail/SplineRail.h"
#include "engine/utils/Easing.h"
#include "externals/nlohmann/json.hpp"
#include "game/camera/PlayCameraController.h"
#include "game/editor/SceneEditContext.h"
#include "game/egg/EggSystem.h"
#include "game/player/Player.h"
#include "game/rail/RailField.h"
#include "game/stage/StageBackdrop.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>

namespace {
    constexpr float kPi = 3.14159265f;
    constexpr const char* kHitSe  = "resources/kamishibai/se_pon.wav";  // スイッチに当たった
    constexpr const char* kFlipSe = "resources/gameover/se_basa.wav";   // カメラが回り込む
    constexpr const char* kSwitchModel = "flipSwitch";
    constexpr const char* kSwitchCoolModel = "flipSwitchCool";
    constexpr const char* kCaveWallModel = "flipCaveWall";
    constexpr const char* kCaveFloorModel = "flipCaveFloor";
    constexpr float kSwitchSize = 1.0f;   // 箱の大きさ(m)
    constexpr float kBobAmp = 0.15f;      // 上下に揺れる幅(m)

    // 確かめる用のキー：環境変数 CG2_DEBUG_KEYS を付けて起動した時だけ（F8＝近くのスイッチで反転）
    bool DebugKeysEnabled(){
        static const bool enabled = []{
            char* value = nullptr;
            size_t length = 0;
            const bool found = _dupenv_s(&value, &length, "CG2_DEBUG_KEYS") == 0 && value;
            free(value);
            return found;
        }();
        return enabled;
    }

    std::string FlipPath(const std::string& stage){ return "resources/map/" + stage + "_flip.json"; }

    const char* SideName(WorldFlip::Side side){
        switch ( side ) {
        case WorldFlip::Side::Front: return "front";
        case WorldFlip::Side::Back:  return "back";
        default:                     return "both";
        }
    }
    WorldFlip::Side SideFromName(const std::string& name){
        if ( name == "front" ) { return WorldFlip::Side::Front; }
        if ( name == "back" ) { return WorldFlip::Side::Back; }
        return WorldFlip::Side::Both;
    }

    Vector3 Lerp(const Vector3& a, const Vector3& b, float t){
        return { a.x + ( b.x - a.x ) * t, a.y + ( b.y - a.y ) * t, a.z + ( b.z - a.z ) * t };
    }

    // 線分 from→to と球が触れるか
    bool SegmentHitsSphere(const Vector3& from, const Vector3& to, const Vector3& center, float radius){
        const Vector3 d { to.x - from.x, to.y - from.y, to.z - from.z };
        const Vector3 m { center.x - from.x, center.y - from.y, center.z - from.z };
        const float len2 = d.x * d.x + d.y * d.y + d.z * d.z;
        float t = len2 > 1e-8f ? ( m.x * d.x + m.y * d.y + m.z * d.z ) / len2 : 0.0f;
        t = Easing::Clamp01(t);
        const Vector3 p { from.x + d.x * t - center.x, from.y + d.y * t - center.y, from.z + d.z * t - center.z };
        return p.x * p.x + p.y * p.y + p.z * p.z <= radius * radius;
    }

    nlohmann::json ToJson(const Vector3& v){ return nlohmann::json::array({ v.x, v.y, v.z }); }
    Vector3 Vec3From(const nlohmann::json& j, const Vector3& fallback){
        if ( !j.is_array() || j.size() < 3 ) { return fallback; }
        return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>() };
    }
}

WorldFlip::WorldFlip() = default;
WorldFlip::~WorldFlip() = default;

void WorldFlip::Initialize(const Context& context, const std::string& stage){
    context_ = context;
    stage_ = stage;

    // スイッチ：紫の箱（使えない間は灰色）。ブロックと同じ紙の箱を別名で読んで色を付ける
    ModelManager* models = ModelManager::GetInstance();
    auto loadTinted = [&](const char* name, const Vector4& color){
        if ( !models->FindModel(name) ) { models->LoadModel(name, "resources/block/craft", "block_1x1x1_layer.obj", true); }
        if ( Model* model = models->FindModel(name) ) {
            if ( model->GetMaterial() ) { model->GetMaterial()->color = color; }
        }
    };
    loadTinted(kSwitchModel, { 0.72f, 0.38f, 1.0f, 1.0f });
    loadTinted(kSwitchCoolModel, { 0.5f, 0.48f, 0.52f, 1.0f });

    AudioManager::GetInstance()->LoadWave(kHitSe);
    AudioManager::GetInstance()->LoadWave(kFlipSe);

    Load();
    BuildCave();
    ResetToFront();
}

// 洞窟：裏ルートのまわり（表のカメラからは背中側）に、段ボールの床・奥の岩肌・紙の岩。裏の間だけ描く
void WorldFlip::BuildCave(){
    ModelManager* models = ModelManager::GetInstance();
    auto makePlane = [&](const char* name, const Vector4& color, bool lit){
        if ( !models->FindModel(name) ) { models->CreatePlaneModel(name, 1.0f, 1.0f); }
        if ( Model* model = models->FindModel(name) ) {
            model->SetTexture("resources/block/white1x1.png"); // 大きな板に紙の画像を貼ると引き伸ばされて筋になるので、色だけ
            if ( Model::Material* material = model->GetMaterial() ) {
                material->color = color;
                material->enableLighting = lit ? 1 : 0;
                material->matte = 1.0f;
            }
        }
        auto obj = Obj3d::Create(name);
        if ( obj ) { obj->SetPipelineType(PipelineType::Object3D_CullNone); }
        return obj;
    };
    // 色は linear（画面では明るく出るので、かなり暗い値にする）
    caveWall_ = makePlane(kCaveWallModel, { 0.012f, 0.009f, 0.02f, 1.0f }, false);
    caveFloor_ = makePlane(kCaveFloorModel, { 0.09f, 0.07f, 0.08f, 1.0f }, true);

    // 紙の岩：裏ルートの奥と手前にぽつぽつ（毎回同じ並びになるよう決まった乱数）
    rocks_.clear();
    unsigned seed = 12345u;
    auto random = [&]{ seed = seed * 1103515245u + 12345u; return ( float ) ( ( seed >> 8 ) & 0xFFFF ) / 65535.0f; };
    for ( int i = 0; i < 26; ++i ) {
        const char* name = ( i % 2 ) ? "paper_rock_01" : "paper_rock_00";
        if ( !models->FindModel(name) ) { continue; }
        auto rock = Obj3d::Create(name);
        if ( !rock ) { continue; }
        const float x = -10.0f + random() * 80.0f;
        const bool beyond = random() < 0.75f; // 多くは、裏のカメラから見て裏ルートの向こう
        const float z = beyond ? -17.0f - random() * 18.0f : -7.0f - random() * 2.0f;
        const float scale = beyond ? 1.4f + random() * 2.6f : 0.8f + random() * 0.8f;
        rock->SetTranslation({ x, 1.95f, z });
        rock->SetScale({ scale, scale * ( 0.8f + random() * 0.9f ), scale });
        rock->SetRotation({ 0.0f, random() * 6.28f, 0.0f });
        rocks_.push_back(std::move(rock));
    }
}

void WorldFlip::Finalize(){
    if ( lightSaved_ ) {
        Obj3dCommon* common = Obj3dCommon::GetInstance();
        if ( common->GetLightData() ) { *common->GetLightData() = savedLight_; }
        if ( common->GetPointLightData() ) { *common->GetPointLightData() = savedPoint_; }
        lightSaved_ = false;
    }
    switches_.clear();
    caveWall_.reset();
    caveFloor_.reset();
    rocks_.clear();
}

void WorldFlip::MakeSwitchObject(Switch& sw){
    sw.obj = Obj3d::Create(kSwitchModel);
}

// ===== 読み書き（resources/map/<ステージ>_flip.json）=====
bool WorldFlip::Load(){
    switches_.clear();
    railSides_.clear();
    std::ifstream file(FlipPath(stage_));
    if ( !file ) { return false; }
    nlohmann::json root;
    try { file >> root; } catch ( ... ) { return false; }

    for ( const auto& entry : root.value("railSides", nlohmann::json::array()) ) {
        const int rail = entry.value("rail", -1);
        if ( rail < 0 ) { continue; }
        if ( rail >= ( int ) railSides_.size() ) { railSides_.resize(rail + 1, ( int ) Side::Both); }
        railSides_[rail] = ( int ) SideFromName(entry.value("side", std::string("both")));
    }
    hitStop_      = root.value("hitStop", hitStop_);
    flipTime_     = root.value("flipTime", flipTime_);
    cooldownTime_ = root.value("cooldown", cooldownTime_);
    radiusGrow_   = root.value("radiusGrow", radiusGrow_);
    hopHeight_    = root.value("hopHeight", hopHeight_);
    backHideZ_    = root.value("backHideZ", backHideZ_);
    if ( root.contains("backLight") ) {
        const auto& backLight = root["backLight"];
        backLightColor_ = Vec3From(backLight.value("color", nlohmann::json()), backLightColor_);
        backLightScale_ = backLight.value("intensity", backLightScale_);
    }
    for ( const auto& entry : root.value("switches", nlohmann::json::array()) ) {
        Switch sw;
        sw.pos = Vec3From(entry.value("pos", nlohmann::json()), {});
        sw.hitRadius = entry.value("hitRadius", sw.hitRadius);
        auto readPoint = [&](const char* key, TransferPoint& point){
            if ( !entry.contains(key) ) { return; }
            point.rail = entry[key].value("rail", -1);
            point.pos = Vec3From(entry[key].value("pos", nlohmann::json()), {});
        };
        readPoint("front", sw.front);
        readPoint("back", sw.back);
        MakeSwitchObject(sw);
        switches_.push_back(std::move(sw));
    }
    return true;
}

bool WorldFlip::Save() const{
    nlohmann::json root;
    nlohmann::json sides = nlohmann::json::array();
    for ( int i = 0; i < ( int ) railSides_.size(); ++i ) {
        if ( railSides_[i] != ( int ) Side::Both ) { sides.push_back({ { "rail", i }, { "side", SideName(( Side ) railSides_[i]) } }); }
    }
    root["railSides"] = sides;
    root["hitStop"] = hitStop_;
    root["flipTime"] = flipTime_;
    root["cooldown"] = cooldownTime_;
    root["radiusGrow"] = radiusGrow_;
    root["hopHeight"] = hopHeight_;
    root["backHideZ"] = backHideZ_;
    root["backLight"] = { { "color", ToJson(backLightColor_) }, { "intensity", backLightScale_ } };
    nlohmann::json list = nlohmann::json::array();
    for ( const Switch& sw : switches_ ) {
        list.push_back({
            { "pos", ToJson(sw.pos) },
            { "hitRadius", sw.hitRadius },
            { "front", { { "rail", sw.front.rail }, { "pos", ToJson(sw.front.pos) } } },
            { "back",  { { "rail", sw.back.rail },  { "pos", ToJson(sw.back.pos) } } },
        });
    }
    root["switches"] = list;
    std::ofstream file(FlipPath(stage_));
    if ( !file ) { return false; }
    file << root.dump(2);
    return true;
}

WorldFlip::Side WorldFlip::RailSide(int rail) const{
    if ( rail < 0 || rail >= ( int ) railSides_.size() ) { return Side::Both; }
    return ( Side ) railSides_[rail];
}

// ===== 進行 =====
void WorldFlip::ResetToFront(){
    if ( phase_ != Phase::None ) { Time::GetInstance()->SetTimeScale(1.0f); }
    phase_ = Phase::None;
    side_ = Side::Front;
    time_ = 0.0f;
    cooldown_ = 0.0f;
    activeSwitch_ = -1;
    progress_ = 0.0f;
    look_ = 0.0f;
    visualOffset_ = {};
    for ( Switch& sw : switches_ ) { sw.spin = 0.0f; }
    if ( context_.camCtrl ) { context_.camCtrl->SetFlip(0.0f, 1.0f, false); }
}

void WorldFlip::BeginFlip(int switchIndex){
    phase_ = Phase::Hit;
    time_ = 0.0f;
    activeSwitch_ = switchIndex;
    toBack_ = ( side_ == Side::Front );
    if ( switchIndex >= 0 && switchIndex < ( int ) switches_.size() ) {
        switches_[switchIndex].spin = 1.0f;
        AudioManager::GetInstance()->PlayWave(kHitSe, false, 0.8f);
    }
    Time::GetInstance()->SetTimeScale(0.0f);
}

// 回り込み始め：スイッチの相手側の乗り換え点へ移る（見た目は元の位置から飛び移る）
void WorldFlip::StartTurn(){
    phase_ = Phase::Turn;
    time_ = 0.0f;
    jumpFrom_ = {};
    Player* player = context_.player;
    if ( player && context_.railField && activeSwitch_ >= 0 && activeSwitch_ < ( int ) switches_.size() ) {
        const auto& rails = context_.railField->GetRails();
        const TransferPoint& point = toBack_ ? switches_[activeSwitch_].back : switches_[activeSwitch_].front;
        if ( point.rail >= 0 && point.rail < ( int ) rails.size() && rails[point.rail].nodes.size() >= 2 ) {
            const Vector3 before = player->GetPosition();
            player->WarpTo(rails, point.rail, rails[point.rail].GetClosestDistance(point.pos));
            const Vector3 after = player->GetPosition();
            jumpFrom_ = { before.x - after.x, before.y - after.y, before.z - after.z };
        }
    }
    side_ = toBack_ ? Side::Back : Side::Front;
    AudioManager::GetInstance()->PlayWave(kFlipSe, false, 0.7f);
}

void WorldFlip::FinishTurn(){
    phase_ = Phase::None;
    cooldown_ = cooldownTime_;
    look_ = side_ == Side::Back ? 1.0f : 0.0f;
    visualOffset_ = {};
    if ( context_.camCtrl ) { context_.camCtrl->SetFlip(look_ * kPi, 1.0f, false); }
    Time::GetInstance()->SetTimeScale(1.0f);
}

bool WorldFlip::Update(bool debugCameraActive){
    const float dt = Time::GetInstance()->GetUnscaledDeltaTime();
    Player* player = context_.player;

    if ( phase_ == Phase::None ) {
        if ( cooldown_ > 0.0f ) { cooldown_ -= dt; }
        // 確かめ用：F8 で一番近いスイッチの反転（卵を当てなくても試せる）
        if ( DebugKeysEnabled() && player && Input::GetInstance()->Triggerkey(DIK_F8) && !switches_.empty() ) {
            int nearest = 0;
            float best = 1e30f;
            const Vector3 p = player->GetPosition();
            for ( int i = 0; i < ( int ) switches_.size(); ++i ) {
                const Vector3& q = switches_[i].pos;
                const float d = ( p.x - q.x ) * ( p.x - q.x ) + ( p.z - q.z ) * ( p.z - q.z );
                if ( d < best ) { best = d; nearest = i; }
            }
            BeginFlip(nearest);
            return true;
        }
        // 裏から「両方」のレールへ合流した → 乗り換えなしで表へ戻す
        if ( side_ == Side::Back && player && player->IsGrounded() && RailSide(player->GetCurrentRail()) == Side::Both ) {
            BeginFlip(-1);
            time_ = hitStop_; // 当たっていないので止めずに回る
        } else {
            return false;
        }
    }

    Time::GetInstance()->SetTimeScale(0.0f); // 反転中はゲームの時間を止める（ヒットストップが戻しても毎フレーム決め直す）
    time_ += dt;
    if ( phase_ == Phase::Hit ) {
        if ( time_ < hitStop_ ) { return true; }
        StartTurn();
    }

    // 回り込み：カメラはプレイヤーを見たまま、弧を描いて180°。途中で少し遠ざかり、表と裏の両方が見える
    progress_ = Easing::Clamp01(time_ / ( std::max )( flipTime_, 0.05f ));
    const float e = Easing::EaseInOutSine(progress_);
    look_ = toBack_ ? e : 1.0f - e;
    const float arc = std::sin(progress_ * kPi);
    visualOffset_ = { jumpFrom_.x * ( 1.0f - e ), jumpFrom_.y * ( 1.0f - e ) + hopHeight_ * arc, jumpFrom_.z * ( 1.0f - e ) };

    if ( context_.camCtrl && context_.camera && context_.railField && player ) {
        const Vector3 base = player->GetPosition();
        const Vector3 focus { base.x + jumpFrom_.x * ( 1.0f - e ), base.y + jumpFrom_.y * ( 1.0f - e ), base.z + jumpFrom_.z * ( 1.0f - e ) };
        context_.camCtrl->SetFlip(look_ * kPi, 1.0f + radiusGrow_ * arc, true);
        context_.camCtrl->Update(context_.camera, focus, true, context_.railField->GetRails(), debugCameraActive, dt); // デバッグカメラ中は動かさない
    }
    // 後ろの卵：止めたまま、見た目の行列だけ今のカメラで（動かさない）
    if ( context_.eggs && player ) {
        const float yaw = player->GetRotation().y;
        context_.eggs->Update(player->GetPosition(), { std::sin(yaw), 0.0f, std::cos(yaw) }, 0.0f);
    }
    if ( progress_ >= 1.0f ) { FinishTurn(); }
    return true;
}

void WorldFlip::CheckEggHits(){
    if ( !context_.eggs || switches_.empty() ) { return; }
    context_.eggs->ResolveHits([&](const Vector3& from, const Vector3& to, float radius) -> bool {
        for ( int i = 0; i < ( int ) switches_.size(); ++i ) {
            const Switch& sw = switches_[i];
            const Vector3 center { sw.pos.x, sw.pos.y + kSwitchSize * 0.5f, sw.pos.z };
            if ( !SegmentHitsSphere(from, to, center, sw.hitRadius + radius) ) { continue; }
            if ( phase_ == Phase::None && cooldown_ <= 0.0f ) { BeginFlip(i); } // クールダウン中は割れるだけ
            return true;
        }
        return false;
    });
}

// ===== 見た目 =====
void WorldFlip::ApplyRails(bool playing){
    if ( !context_.railField ) { return; }
    auto& rails = context_.railField->GetRailsMutable();
    for ( int i = 0; i < ( int ) rails.size(); ++i ) {
        SplineRail& rail = rails[i];
        const Side side = RailSide(i);
        rail.worldSide = ( int ) side;
        if ( !playing || side == Side::Both ) {
            rail.sideBlocked = false;
            rail.sideAnim = 1.0f;
            continue;
        }
        rail.sideBlocked = side != side_;
        rail.sideAnim = side == Side::Back ? look_ : 1.0f - look_; // 反転の進み具合で、相手の面の道が生え、元の面の道が沈む
    }
}

void WorldFlip::ApplyBackdrop(bool playing){
    if ( !context_.backdrop ) { return; }
    // 裏へ回り始めたら、手前（カメラ側）の草・地面を消して洞窟の床に置き換える
    //   （回り始めはカメラの背中側なので、消えた瞬間は見えない）
    const bool back = playing && look_ > 0.0f;
    std::vector<char> hidden;
    if ( back ) {
        const auto& items = context_.backdrop->Items();
        hidden.assign(items.size(), 0);
        for ( int i = 0; i < ( int ) items.size(); ++i ) {
            Vector3 lo, hi;
            if ( context_.backdrop->WorldBounds(i, lo, hi) && hi.z < backHideZ_ ) { hidden[i] = 1; }
        }
    }
    context_.backdrop->SetRuntimeHidden(hidden);
}

void WorldFlip::ApplyLight(){
    Obj3dCommon* common = Obj3dCommon::GetInstance();
    Obj3dCommon::DirectionalLightData* light = common->GetLightData();
    Obj3dCommon::PointLight* point = common->GetPointLightData();
    if ( !light || !point ) { return; }
    if ( look_ <= 0.0f ) {
        if ( lightSaved_ ) { *light = savedLight_; *point = savedPoint_; lightSaved_ = false; } // 表に戻った：元の光
        return;
    }
    if ( !lightSaved_ ) { savedLight_ = *light; savedPoint_ = *point; lightSaved_ = true; }
    // 恐竜のまわりの明かり（松明の代わり）。裏らしさに合わせて灯る
    if ( context_.player ) {
        const Vector3 p = context_.player->GetPosition();
        point->position = { p.x + visualOffset_.x, p.y + visualOffset_.y + 2.2f, p.z + visualOffset_.z };
        point->color = { lanternColor_.x, lanternColor_.y, lanternColor_.z, 1.0f };
        point->intensity = lanternIntensity_ * look_;
        point->radius = 9.0f;
        point->decay = 1.5f;
    }
    *light = savedLight_;
    const Vector4& from = savedLight_.lights[0].color;
    light->lights[0].color = { from.x + ( backLightColor_.x - from.x ) * look_, from.y + ( backLightColor_.y - from.y ) * look_,
                               from.z + ( backLightColor_.z - from.z ) * look_, from.w };
    light->lights[0].intensity = savedLight_.lights[0].intensity * ( 1.0f + ( backLightScale_ - 1.0f ) * look_ );
}

void WorldFlip::UpdateVisuals(bool playing){
    if ( !playing && phase_ != Phase::None ) { ResetToFront(); }
    ApplyRails(playing);
    ApplyBackdrop(playing);
    ApplyLight();

    const float dt = Time::GetInstance()->GetUnscaledDeltaTime();
    const float now = Time::GetInstance()->GetUnscaledTotalTime();
    const bool cooling = cooldown_ > 0.0f || phase_ != Phase::None;
    for ( int i = 0; i < ( int ) switches_.size(); ++i ) {
        Switch& sw = switches_[i];
        if ( !sw.obj ) { continue; }
        sw.spin = ( std::max )( 0.0f, sw.spin - dt / 0.45f );
        const float bob = std::sin(now * 2.2f + ( float ) i) * kBobAmp;
        const float pop = 1.0f + 0.25f * std::sin(sw.spin * kPi); // 当たった時に少しふくらむ
        // くるっと1回転（当たった時）＋ゆっくり回り続ける
        const float yaw = now * 0.8f + ( 1.0f - sw.spin ) * ( sw.spin > 0.0f ? 2.0f * kPi : 0.0f );
        sw.obj->SetModel(ModelManager::GetInstance()->FindModel(cooling ? kSwitchCoolModel : kSwitchModel));
        sw.obj->SetTranslation({ sw.pos.x, sw.pos.y + bob - ( pop - 1.0f ) * kSwitchSize * 0.5f, sw.pos.z });
        sw.obj->SetScale({ kSwitchSize * pop, kSwitchSize * pop, kSwitchSize * pop });
        sw.obj->SetRotation({ 0.0f, yaw, 0.0f });
        sw.obj->Update();
    }

    // 洞窟：奥の岩肌（裏のカメラから見て裏ルートの向こう）と床
    if ( caveWall_ ) {
        caveWall_->SetTranslation({ 60.0f, 10.0f, -42.0f });
        caveWall_->SetScale({ 400.0f, 60.0f, 1.0f });
        caveWall_->Update();
    }
    if ( caveFloor_ ) {
        caveFloor_->SetTranslation({ 60.0f, 1.9f, -22.0f });
        caveFloor_->SetRotation({ kPi * 0.5f, 0.0f, 0.0f });
        caveFloor_->SetScale({ 400.0f, 42.0f, 1.0f });
        caveFloor_->Update();
    }
    for ( auto& rock : rocks_ ) { rock->Update(); }

    // 確かめ用：スイッチの当たり（球）と、表・裏の乗り換え点。所属のあるレールは色の線
    if ( showDebug_ && ( !playing || showInPlay_ ) && context_.railField ) {
        const auto& rails = context_.railField->GetRails();
        for ( int i = 0; i < ( int ) switches_.size(); ++i ) {
            const Switch& sw = switches_[i];
            const Vector3 center { sw.pos.x, sw.pos.y + kSwitchSize * 0.5f, sw.pos.z };
            DebugDraw::GetInstance()->Sphere(center, sw.hitRadius, i == selected_ ? Vector4 { 1.0f, 0.4f, 1.0f, 0.8f } : Vector4 { 0.7f, 0.4f, 1.0f, 0.4f });
            auto pointOnRail = [&](const TransferPoint& point, Vector3& out){
                if ( point.rail < 0 || point.rail >= ( int ) rails.size() || rails[point.rail].nodes.size() < 2 ) { return false; }
                out = rails[point.rail].GetPositionByDistance(rails[point.rail].GetClosestDistance(point.pos));
                return true;
            };
            Vector3 front, back;
            const bool hasFront = pointOnRail(sw.front, front);
            const bool hasBack = pointOnRail(sw.back, back);
            if ( hasFront ) { DebugDraw::GetInstance()->Sphere(front, 0.35f, { 1.0f, 0.9f, 0.3f, 0.9f }, 8); DebugDraw::GetInstance()->Line(center, front, { 1.0f, 0.9f, 0.3f, 0.8f }); }
            if ( hasBack ) { DebugDraw::GetInstance()->Sphere(back, 0.35f, { 0.4f, 0.6f, 1.0f, 0.9f }, 8); DebugDraw::GetInstance()->Line(center, back, { 0.4f, 0.6f, 1.0f, 0.8f }); }
            if ( hasFront ) { DebugDraw::GetInstance()->Line(front, { front.x, front.y + 2.0f, front.z }, { 1.0f, 0.9f, 0.3f, 1.0f }); }
            if ( hasBack ) { DebugDraw::GetInstance()->Line(back, { back.x, back.y + 2.0f, back.z }, { 0.4f, 0.6f, 1.0f, 1.0f }); }
            // 表 ⇔ 裏 の飛び移る道すじ（白い弧）
            if ( hasFront && hasBack ) {
                Vector3 prev = front;
                for ( int k = 1; k <= 12; ++k ) {
                    const float t = ( float ) k / 12.0f;
                    Vector3 q = Lerp(front, back, t);
                    q.y += hopHeight_ * std::sin(t * kPi);
                    DebugDraw::GetInstance()->Line(prev, q, { 1.0f, 1.0f, 1.0f, 0.7f });
                    prev = q;
                }
            }
        }
        // 所属のあるレール：裏＝青 / 表だけ＝黄色の線を、レールの少し上に重ねる
        for ( int r = 0; r < ( int ) rails.size(); ++r ) {
            const Side side = RailSide(r);
            if ( side == Side::Both || rails[r].nodes.size() < 2 ) { continue; }
            const Vector4 color = side == Side::Back ? Vector4 { 0.35f, 0.55f, 1.0f, 1.0f } : Vector4 { 1.0f, 0.85f, 0.25f, 1.0f };
            const float length = rails[r].GetLength();
            Vector3 prev = rails[r].GetPositionByDistance(0.0f);
            for ( float d = 0.5f; d <= length + 0.25f; d += 0.5f ) {
                const Vector3 q = rails[r].GetPositionByDistance(( std::min )( d, length ));
                DebugDraw::GetInstance()->Line({ prev.x, prev.y + 0.3f, prev.z }, { q.x, q.y + 0.3f, q.z }, color);
                prev = q;
            }
        }
    }
}

// Game View の名札：スイッチの番号、表・裏の乗り換え点（どのレールか・おかしい所）、所属のあるレール
void WorldFlip::DrawGameViewLabels(const SceneEditContext& context){
#ifdef USE_IMGUI
    const bool playing = !context.editing;
    if ( !showDebug_ || ( playing && !showInPlay_ ) || !context.gameView || !context.rails ) { return; }
    const auto& view = context.View();
    const auto& rails = context.Rails();
    if ( !ImGui::Begin("Game View") ) { ImGui::End(); return; }
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect({ view.imgMin.x, view.imgMin.y }, { view.imgMin.x + view.imgSize.x, view.imgMin.y + view.imgSize.y }, true);
    auto label = [&](const Vector3& world, const char* text, ImU32 background){
        Vector2 screen;
        if ( !context.ProjectToScreen(world, screen) ) { return; }
        const ImVec2 size = ImGui::CalcTextSize(text);
        const ImVec2 pos { screen.x - size.x * 0.5f, screen.y - size.y };
        draw->AddRectFilled({ pos.x - 4.0f, pos.y - 2.0f }, { pos.x + size.x + 4.0f, pos.y + size.y + 2.0f }, background, 4.0f);
        draw->AddText(pos, IM_COL32(255, 255, 255, 255), text);
    };
    auto pointOnRail = [&](const TransferPoint& point, Vector3& out){
        if ( point.rail < 0 || point.rail >= ( int ) rails.size() || rails[point.rail].nodes.size() < 2 ) { return false; }
        out = rails[point.rail].GetPositionByDistance(rails[point.rail].GetClosestDistance(point.pos));
        return true;
    };
    const ImU32 kWarn = IM_COL32(190, 40, 40, 230);
    char text[128];
    for ( int i = 0; i < ( int ) switches_.size(); ++i ) {
        const Switch& sw = switches_[i];
        snprintf(text, sizeof(text), "？スイッチ %d", i);
        label({ sw.pos.x, sw.pos.y + kSwitchSize + 0.6f, sw.pos.z }, text, i == selected_ ? IM_COL32(150, 60, 200, 230) : IM_COL32(90, 40, 120, 200));
        Vector3 at;
        if ( pointOnRail(sw.front, at) ) {
            const bool wrong = RailSide(sw.front.rail) == Side::Back;
            snprintf(text, sizeof(text), "%d 表の乗り換え（レール%d）%s", i, sw.front.rail, wrong ? " ※裏のレール" : "");
            label({ at.x, at.y + 2.3f, at.z }, text, wrong ? kWarn : IM_COL32(160, 120, 0, 220));
        } else {
            snprintf(text, sizeof(text), "%d 表の乗り換え：レールが無い", i);
            label({ sw.pos.x, sw.pos.y - 0.4f, sw.pos.z }, text, kWarn);
        }
        if ( pointOnRail(sw.back, at) ) {
            const bool wrong = RailSide(sw.back.rail) != Side::Back;
            snprintf(text, sizeof(text), "%d 裏の乗り換え（レール%d）%s", i, sw.back.rail, wrong ? " ※裏になっていない" : "");
            label({ at.x, at.y + 2.3f, at.z }, text, wrong ? kWarn : IM_COL32(40, 80, 170, 220));
        } else {
            snprintf(text, sizeof(text), "%d 裏の乗り換え：レールが無い", i);
            label({ sw.pos.x, sw.pos.y - 0.8f, sw.pos.z }, text, kWarn);
        }
    }
    for ( int r = 0; r < ( int ) rails.size(); ++r ) {
        const Side side = RailSide(r);
        if ( side == Side::Both || rails[r].nodes.size() < 2 ) { continue; }
        const Vector3 mid = rails[r].GetPositionByDistance(rails[r].GetLength() * 0.5f);
        snprintf(text, sizeof(text), "%sのレール %d", side == Side::Back ? "裏" : "表だけ", r);
        label({ mid.x, mid.y + 0.9f, mid.z }, text, side == Side::Back ? IM_COL32(40, 80, 170, 200) : IM_COL32(160, 120, 0, 200));
    }
    draw->PopClipRect();
    ImGui::End();
#else
    ( void ) context;
#endif
}

void WorldFlip::Draw(){
    for ( Switch& sw : switches_ ) {
        if ( sw.obj ) { sw.obj->Draw(); }
    }
    if ( look_ > 0.0f ) { // 洞窟は裏へ回り始めてから（表のカメラからは背中側）
        if ( caveFloor_ ) { caveFloor_->Draw(); }
        if ( caveWall_ ) { caveWall_->Draw(); }
        for ( auto& rock : rocks_ ) { rock->Draw(); }
    }
}

// ===== インスペクター =====
void WorldFlip::DrawInspector(bool playing){
#ifdef USE_IMGUI
    if ( !ImGui::CollapsingHeader("裏世界（反転）") ) { return; }
    static const char* kSideLabels[] = { "両方", "表", "裏" };
    ImGui::Text("今の面：%s  %s  クールダウン %.2f", side_ == Side::Back ? "裏" : "表",
                phase_ == Phase::None ? "" : "（反転中）", ( std::max )( cooldown_, 0.0f ));
    if ( !playing ) { ImGui::BeginDisabled(); }
    if ( ImGui::Button("反転を試す（選んでいるスイッチ）") && phase_ == Phase::None && selected_ >= 0 && selected_ < ( int ) switches_.size() ) {
        BeginFlip(selected_);
    }
    if ( !playing ) { ImGui::EndDisabled(); }
    ImGui::SameLine();
    if ( ImGui::Button("表へ戻す") ) { ResetToFront(); }
    ImGui::Checkbox("スイッチ・乗り換え点・裏のレールを表示", &showDebug_);
    ImGui::SameLine();
    ImGui::Checkbox("プレイ中も", &showInPlay_);
    ImGui::TextDisabled("黄＝表の乗り換え点 / 青＝裏の乗り換え点・裏のレール / 白い弧＝飛び移る道すじ");

    if ( ImGui::TreeNode("時間・カメラ") ) {
        ImGui::SliderFloat("ヒットストップ(秒)", &hitStop_, 0.0f, 0.6f);
        ImGui::SliderFloat("反転の時間(秒)", &flipTime_, 0.4f, 2.0f);
        ImGui::SliderFloat("クールダウン(秒)", &cooldownTime_, 0.0f, 4.0f);
        ImGui::SliderFloat("回る途中に広げる割合", &radiusGrow_, 0.0f, 1.0f);
        ImGui::SliderFloat("飛び移る高さ(m)", &hopHeight_, 0.0f, 6.0f);
        ImGui::TreePop();
    }
    if ( ImGui::TreeNode("裏の見た目") ) {
        ImGui::DragFloat("裏の間に消す手前の背景(z がこれより小さい)", &backHideZ_, 0.1f);
        ImGui::ColorEdit3("恐竜のまわりの明かり", &lanternColor_.x);
        ImGui::SliderFloat("明かりの強さ", &lanternIntensity_, 0.0f, 5.0f);
        ImGui::ColorEdit3("裏の光の色", &backLightColor_.x);
        ImGui::SliderFloat("裏の光の強さ(倍)", &backLightScale_, 0.0f, 1.5f);
        ImGui::TreePop();
    }

    if ( ImGui::TreeNode("レールの所属") && context_.railField ) {
        const int count = ( int ) context_.railField->GetRails().size();
        if ( ( int ) railSides_.size() < count ) { railSides_.resize(count, ( int ) Side::Both); }
        if ( ImGui::BeginTable("flipRails", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit) ) {
            for ( int i = 0; i < count; ++i ) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("レール %d", i);
                ImGui::TableNextColumn();
                ImGui::PushID(i);
                ImGui::SetNextItemWidth(90.0f);
                ImGui::Combo("##side", &railSides_[i], kSideLabels, 3);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::TreePop();
    }

    if ( ImGui::TreeNode("「？」スイッチ") ) {
        if ( ImGui::Button("追加") ) {
            Switch sw;
            if ( context_.player ) {
                const Vector3 p = context_.player->GetPosition();
                sw.pos = { p.x, p.y + 2.6f, p.z };
            }
            MakeSwitchObject(sw);
            switches_.push_back(std::move(sw));
            selected_ = ( int ) switches_.size() - 1;
        }
        ImGui::SameLine();
        if ( ImGui::Button("消す") && selected_ >= 0 && selected_ < ( int ) switches_.size() ) {
            switches_.erase(switches_.begin() + selected_);
            selected_ = ( std::max )( 0, selected_ - 1 );
        }
        for ( int i = 0; i < ( int ) switches_.size(); ++i ) {
            char label[32];
            snprintf(label, sizeof(label), "スイッチ %d", i);
            if ( ImGui::Selectable(label, selected_ == i) ) { selected_ = i; }
        }
        if ( selected_ >= 0 && selected_ < ( int ) switches_.size() ) {
            Switch& sw = switches_[selected_];
            ImGui::DragFloat3("位置（箱の底）", &sw.pos.x, 0.05f);
            ImGui::SliderFloat("卵の当たり半径", &sw.hitRadius, 0.3f, 3.0f);
            auto pointUI = [&](const char* name, TransferPoint& point){
                ImGui::PushID(name);
                ImGui::Text("%s の乗り換え点", name);
                ImGui::SetNextItemWidth(90.0f);
                ImGui::InputInt("レール", &point.rail);
                ImGui::DragFloat3("位置", &point.pos.x, 0.05f);
                if ( context_.player && ImGui::Button("今のプレイヤーの所にする") ) {
                    point.rail = context_.player->GetCurrentRail();
                    point.pos = context_.player->GetPosition();
                }
                // 確かめ：レールがあるか・所属が合っているか・レールからどれだけ離れているか
                if ( context_.railField ) {
                    const auto& rails = context_.railField->GetRails();
                    if ( point.rail < 0 || point.rail >= ( int ) rails.size() || rails[point.rail].nodes.size() < 2 ) {
                        ImGui::TextColored({ 1.0f, 0.4f, 0.4f, 1.0f }, "レール %d がありません", point.rail);
                    } else {
                        const float dist = rails[point.rail].GetClosestDistance(point.pos);
                        const Vector3 on = rails[point.rail].GetPositionByDistance(dist);
                        const float gap = std::sqrt(( on.x - point.pos.x ) * ( on.x - point.pos.x ) + ( on.z - point.pos.z ) * ( on.z - point.pos.z ));
                        const Side side = RailSide(point.rail);
                        const bool wantBack = ( &point == &sw.back );
                        const bool ok = wantBack ? side == Side::Back : side != Side::Back;
                        ImGui::TextColored(ok ? ImVec4 { 0.6f, 1.0f, 0.6f, 1.0f } : ImVec4 { 1.0f, 0.4f, 0.4f, 1.0f },
                                           "→ レール %d（%s）の %.1fm 地点%s", point.rail, kSideLabels[( int ) side], dist,
                                           ok ? "" : ( wantBack ? "：裏のレールにしてください" : "：裏のレールになっています" ));
                        if ( gap > 1.0f ) { ImGui::TextColored({ 1.0f, 0.8f, 0.3f, 1.0f }, "  位置がレールから %.1fm 離れています", gap); }
                    }
                }
                ImGui::PopID();
            };
            pointUI("表", sw.front);
            pointUI("裏", sw.back);
        }
        ImGui::TreePop();
    }

    if ( ImGui::Button("保存") ) { Save(); }
    ImGui::SameLine();
    if ( ImGui::Button("読み直す") ) { Load(); ResetToFront(); }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", FlipPath(stage_).c_str());
#else
    ( void ) playing;
#endif
}
