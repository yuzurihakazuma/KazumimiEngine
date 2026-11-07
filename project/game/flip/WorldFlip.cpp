#include "game/flip/WorldFlip.h"

#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/audio/AudioManager.h"
#include "engine/graphics/PipelineType.h"
#include "engine/postEffect/PostEffect.h"
#include "engine/base/Input.h"
#include "engine/base/TimeManager.h"
#include "engine/camera/Camera.h"
#include "engine/graphics/DebugDraw.h"
#include "engine/rail/SplineRail.h"
#include "engine/utils/Easing.h"
#include "engine/utils/Level/RailEditor.h"
#include "game/camera/PlayCameraController.h"
#include "game/editor/SceneEditContext.h"
#include "game/egg/EggSystem.h"
#include "game/flip/FlipDial.h"
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
    constexpr float kBobAmp = 0.15f;      // スイッチが上下に揺れる幅(m)
    constexpr const char* kEveningSkyModel = "flipEveningSkyDome";
    constexpr const char* kEveningSkyTexture = "resources/flip/sky_evening.png"; // tools/flip/make_flip_assets.py が作る

    // 確かめる用のキー：環境変数 CG2_DEBUG_KEYS を付けて起動した時だけ（F8＝カメラを反転）
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
    FlipDial::LoadModels();
    // 夕焼けの空：カメラのまわりを囲む筒（ふた無しの円柱。光なし・内側から見るので両面）。板1枚だと、回る途中に端が見えていた
    ModelManager* models = ModelManager::GetInstance();
    if ( !models->FindModel(kEveningSkyModel) ) { models->CreateCylinderModel(kEveningSkyModel, 48, 1.0f, 1.0f); }
    if ( Model* model = models->FindModel(kEveningSkyModel) ) {
        model->SetTexture(kEveningSkyTexture);
        if ( Model::Material* material = model->GetMaterial() ) {
            material->color = { 1.0f, 1.0f, 1.0f, 1.0f };
            material->enableLighting = 0;
            material->matte = 1.0f;
        }
    }
    eveningSky_ = Obj3d::Create(kEveningSkyModel);
    if ( eveningSky_ ) { eveningSky_->SetPipelineType(PipelineType::Object3D_CullNone); }
    AudioManager::GetInstance()->LoadWave(kHitSe);
    AudioManager::GetInstance()->LoadWave(kFlipSe);
    Load();
    ResetToFront();
}

void WorldFlip::Finalize(){
    if ( context_.backdrop ) { context_.backdrop->SetRuntimeHidden({}); }
    RestoreDaylight();
    switches_.clear();
    eveningSky_.reset();
}

void WorldFlip::MakeSwitchObject(Switch& sw){
    sw.dial = std::make_unique<FlipDial>();
    sw.dial->Initialize();
}

// ===== 読み書き（resources/map/<ステージ>_flip.json）=====
bool WorldFlip::Load(){
    switches_.clear();
    railSides_.clear();
    points_.clear();
    std::ifstream file(FlipPath(stage_));
    if ( !file ) { savedJson_ = SaveData().dump(); return false; }
    nlohmann::json root;
    try { file >> root; } catch ( ... ) { return false; }

    for ( const auto& entry : root.value("railSides", nlohmann::json::array()) ) {
        const int rail = entry.value("rail", -1);
        if ( rail >= 0 ) { SetRailSide(rail, SideFromName(entry.value("side", std::string("both")))); }
    }
    hitStop_      = root.value("hitStop", hitStop_);
    flipTime_     = root.value("flipTime", flipTime_);
    cooldownTime_ = root.value("cooldown", cooldownTime_);
    radiusGrow_   = root.value("radiusGrow", radiusGrow_);
    occludeGap_   = root.value("occludeGap", occludeGap_);
    if ( root.contains("evening") ) {
        const auto& evening = root["evening"];
        eveningOnBack_ = evening.value("enabled", eveningOnBack_);
        eveningStrength_ = evening.value("strength", eveningStrength_);
        eveningLight_ = Vec3From(evening.value("light", nlohmann::json()), eveningLight_);
        eveningTint_ = evening.value("tint", eveningTint_);
    }
    for ( const auto& entry : root.value("points", nlohmann::json::array()) ) {
        FlipPoint point;
        point.rail = entry.value("rail", -1);
        point.dist = entry.value("dist", 0.0f);
        point.mode = std::clamp(entry.value("mode", 2), 0, 2);
        point.radius = entry.value("radius", point.radius);
        points_.push_back(point);
    }
    for ( const auto& entry : root.value("switches", nlohmann::json::array()) ) {
        Switch sw;
        sw.pos = Vec3From(entry.value("pos", nlohmann::json()), {});
        sw.rail = entry.value("rail", -1);
        sw.dist = entry.value("dist", 0.0f);
        sw.height = entry.value("height", sw.height);
        sw.hitRadius = entry.value("hitRadius", sw.hitRadius);
        sw.linkRail = entry.value("linkRail", -1);
        sw.backRail = entry.value("backRail", -1);
        MakeSwitchObject(sw);
        switches_.push_back(std::move(sw));
    }
    savedJson_ = SaveData().dump();
    return true;
}

nlohmann::json WorldFlip::SaveData() const{
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
    root["occludeGap"] = occludeGap_;
    root["evening"] = { { "enabled", eveningOnBack_ }, { "strength", eveningStrength_ }, { "light", ToJson(eveningLight_) }, { "tint", eveningTint_ } };
    nlohmann::json list = nlohmann::json::array();
    for ( const Switch& sw : switches_ ) {
        list.push_back({
            { "rail", sw.rail },
            { "dist", sw.dist },
            { "height", sw.height },
            { "pos", ToJson(sw.pos) },
            { "hitRadius", sw.hitRadius },
            { "linkRail", sw.linkRail },
            { "backRail", sw.backRail },
        });
    }
    root["switches"] = list;
    nlohmann::json points = nlohmann::json::array();
    for ( const FlipPoint& point : points_ ) {
        points.push_back({ { "rail", point.rail }, { "dist", point.dist }, { "mode", point.mode }, { "radius", point.radius } });
    }
    root["points"] = points;
    return root;
}

bool WorldFlip::Save(){
    const nlohmann::json root = SaveData();
    std::ofstream file(FlipPath(stage_));
    if ( !file ) { return false; }
    file << root.dump(2);
    savedJson_ = root.dump();
    return true;
}

WorldFlip::Side WorldFlip::RailSide(int rail) const{
    if ( rail < 0 || rail >= ( int ) railSides_.size() ) { return Side::Both; }
    return ( Side ) railSides_[rail];
}

void WorldFlip::SetRailSide(int rail, Side side){
    if ( rail < 0 ) { return; }
    if ( rail >= ( int ) railSides_.size() ) { railSides_.resize(rail + 1, ( int ) Side::Both); }
    railSides_[rail] = ( int ) side;
}

// ===== 進行 =====
void WorldFlip::ResetToFront(){
    if ( phase_ != Phase::None ) { Time::GetInstance()->SetTimeScale(1.0f); }
    phase_ = Phase::None;
    back_ = false;
    time_ = 0.0f;
    cooldown_ = 0.0f;
    look_ = 0.0f;
    lookFrom_ = 0.0f;
    lastRail_ = -1; // 次のフレームに、乗っているレールの向きを見直す
    for ( FlipPoint& point : points_ ) { point.inside = false; } // 中で始めたら、そのポイントのとおりにする
    for ( Switch& sw : switches_ ) { sw.spin = 0.0f; }
    if ( context_.camCtrl ) { context_.camCtrl->SetFlip(0.0f, 1.0f, false); }
}

void WorldFlip::BeginTurn(bool withHit, int switchIndex){
    back_ = !back_;
    lookFrom_ = look_;
    time_ = 0.0f;
    phase_ = withHit ? Phase::Hit : Phase::Turn;
    if ( switchIndex >= 0 && switchIndex < ( int ) switches_.size() ) { switches_[switchIndex].spin = 1.0f; }
    AudioManager::GetInstance()->PlayWave(withHit ? kHitSe : kFlipSe, false, withHit ? 0.8f : 0.6f);
    Time::GetInstance()->SetTimeScale(0.0f);
}

void WorldFlip::FinishTurn(){
    phase_ = Phase::None;
    cooldown_ = cooldownTime_;
    look_ = back_ ? 1.0f : 0.0f;
    if ( context_.camCtrl ) { context_.camCtrl->SetFlip(look_ * kPi, 1.0f, false); }
    Time::GetInstance()->SetTimeScale(1.0f);
}

bool WorldFlip::Update(bool debugCameraActive){
    const float dt = Time::GetInstance()->GetUnscaledDeltaTime();
    Player* player = context_.player;

    if ( phase_ == Phase::None ) {
        if ( cooldown_ > 0.0f ) { cooldown_ -= dt; }
        // 確かめ用：F8 でカメラを反転（一番近いスイッチが回る）
        if ( DebugKeysEnabled() && Input::GetInstance()->Triggerkey(DIK_F8) ) {
            int nearest = -1;
            float best = 1e30f;
            if ( player ) {
                const Vector3 p = player->GetPosition();
                for ( int i = 0; i < ( int ) switches_.size(); ++i ) {
                    const Vector3& q = switches_[i].pos;
                    const float d = ( p.x - q.x ) * ( p.x - q.x ) + ( p.z - q.z ) * ( p.z - q.z );
                    if ( d < best ) { best = d; nearest = i; }
                }
            }
            BeginTurn(nearest >= 0, nearest);
        }
        // 切り替えポイントに入った：そのとおりに回る
        else if ( UpdatePoints() ) {}
        // 歩いて別のレールへ乗り移った：そのレールの向きが今のカメラと違えば回る（「変えない」のレールでは回らない）
        else if ( player && player->IsGrounded() ) {
            const int rail = player->GetCurrentRail();
            if ( rail != lastRail_ ) {
                lastRail_ = rail;
                const Side side = RailSide(rail);
                if ( ( side == Side::Back && !back_ ) || ( side == Side::Front && back_ ) ) { BeginTurn(false, -1); }
            }
        }
        if ( phase_ == Phase::None ) { return false; }
    }

    Time::GetInstance()->SetTimeScale(0.0f); // 回り込み中はゲームの時間を止める（ヒットストップが戻しても毎フレーム決め直す）
    time_ += dt;
    if ( phase_ == Phase::Hit ) {
        if ( time_ < hitStop_ ) { return true; }
        phase_ = Phase::Turn;
        time_ = 0.0f;
        AudioManager::GetInstance()->PlayWave(kFlipSe, false, 0.6f);
    }

    // 回り込み：カメラはプレイヤーを見たまま、弧を描いて180°。途中で少し遠ざかる
    const float progress = Easing::Clamp01(time_ / ( std::max )( flipTime_, 0.05f ));
    const float e = Easing::EaseInOutSine(progress);
    look_ = lookFrom_ + ( ( back_ ? 1.0f : 0.0f ) - lookFrom_ ) * e;
    if ( context_.camCtrl && context_.camera && context_.railField && player ) {
        context_.camCtrl->SetFlip(look_ * kPi, 1.0f + radiusGrow_ * std::sin(progress * kPi), true);
        context_.camCtrl->Update(context_.camera, player->GetPosition(), true, context_.railField->GetRails(), debugCameraActive, dt);
    }
    // 後ろの卵：止めたまま、見た目の行列だけ今のカメラで（動かさない）
    if ( context_.eggs && player ) {
        const float yaw = player->GetRotation().y;
        context_.eggs->Update(player->GetPosition(), { std::sin(yaw), 0.0f, std::cos(yaw) }, 0.0f);
    }
    if ( progress >= 1.0f ) { FinishTurn(); }
    return true;
}

// 切り替えポイント：入った瞬間だけ見る（中で卵のスイッチを使って向きを変えても、出て入り直すまで戻さない）
bool WorldFlip::UpdatePoints(){
    if ( !context_.player ) { return false; }
    UpdatePointAnchors(); // 位置を今のレールで（プレイ開始の最初のフレームはまだ求まっていない）
    const Vector3 p = context_.player->GetPosition();
    bool turned = false;
    for ( FlipPoint& point : points_ ) {
        const float dx = p.x - point.pos.x, dz = p.z - point.pos.z;
        const bool inside = point.valid && dx * dx + dz * dz <= point.radius * point.radius && std::abs(p.y - point.pos.y) < 2.5f;
        if ( inside && !point.inside && !turned ) {
            const bool wantBack = point.mode == 0 ? !back_ : point.mode == 2;
            if ( wantBack != back_ ) { BeginTurn(false, -1); turned = true; }
        }
        point.inside = inside;
    }
    return turned;
}

void WorldFlip::UpdatePointAnchors(){
    if ( !context_.railField ) { return; }
    const auto& rails = context_.railField->GetRails();
    for ( FlipPoint& point : points_ ) {
        point.valid = point.rail >= 0 && point.rail < ( int ) rails.size() && rails[point.rail].nodes.size() >= 2;
        if ( !point.valid ) { continue; }
        point.dist = std::clamp(point.dist, 0.0f, rails[point.rail].GetLength());
        point.pos = rails[point.rail].GetPositionByDistance(point.dist);
    }
}

void WorldFlip::CheckEggHits(){
    if ( !context_.eggs || switches_.empty() ) { return; }
    context_.eggs->ResolveHits([&](const Vector3& from, const Vector3& to, float radius) -> bool {
        for ( int i = 0; i < ( int ) switches_.size(); ++i ) {
            const Switch& sw = switches_[i];
            const Vector3 center { sw.pos.x, sw.pos.y + FlipDial::kCenterAbove, sw.pos.z };
            if ( !SegmentHitsSphere(from, to, center, sw.hitRadius + radius) ) { continue; }
            if ( phase_ == Phase::None && cooldown_ <= 0.0f ) { BeginTurn(true, i); } // 使えない間は割れるだけ
            return true;
        }
        return false;
    });
}

// ===== 見た目 =====
// 裏から見る間：恐竜（編集中は見ている所）よりカメラ側にある、背の高い背景を隠す（木や丘が画面をふさがないように）
void WorldFlip::ApplyBackdrop(bool playing){
    if ( !context_.backdrop ) { return; }
    Vector3 pivot {};
    bool hide = false;
    if ( playing && look_ > 0.5f && context_.player ) { hide = true; pivot = context_.player->GetPosition(); }
    if ( !playing && previewBack_ ) { hide = true; pivot = previewPivot_; }
    std::vector<char> hidden;
    if ( hide ) {
        const auto& items = context_.backdrop->Items();
        hidden.assign(items.size(), 0);
        for ( int i = 0; i < ( int ) items.size(); ++i ) {
            Vector3 lo, hi;
            if ( context_.backdrop->WorldBounds(i, lo, hi) && lo.z > pivot.z + occludeGap_ && hi.y > pivot.y + 0.6f ) { hidden[i] = 1; }
        }
    }
    context_.backdrop->SetRuntimeHidden(hidden);
}

void WorldFlip::UpdateVisuals(bool playing){
    if ( !playing && phase_ != Phase::None ) { ResetToFront(); }
    if ( playing ) { previewBack_ = false; }
    UpdateSwitchAnchors(); // スイッチをレールの上へ（レールを動かしたら付いてくる）
    UpdatePointAnchors();
    ApplyBackdrop(playing);
    // 夕方：カメラの回り具合のとおり（編集中は「裏のカメラ」で見ている時）
    const float backLook = playing ? look_ : ( previewBack_ ? 1.0f : 0.0f );
    ApplyEvening(eveningOnBack_ ? backLook * eveningStrength_ : 0.0f);

    const float dt = Time::GetInstance()->GetUnscaledDeltaTime();
    const float now = Time::GetInstance()->GetUnscaledTotalTime();
    const bool cooling = cooldown_ > 0.0f || phase_ != Phase::None;
    // 文字盤は今のカメラの方を向く（回り込みと一緒に回る。編集中に裏から見ている時も裏を向く）
    const float facingLook = playing ? look_ : ( previewBack_ ? 1.0f : 0.0f );
    for ( int i = 0; i < ( int ) switches_.size(); ++i ) {
        Switch& sw = switches_[i];
        if ( !sw.dial ) { continue; }
        sw.spin = ( std::max )( 0.0f, sw.spin - dt / 0.6f );
        const float bob = std::sin(now * 2.2f + ( float ) i) * kBobAmp;
        const float yaw = facingLook * kPi + 0.12f * std::sin(now * 0.9f + ( float ) i * 1.7f);
        const float groundY = sw.rail >= 0 ? sw.pos.y - sw.height : sw.pos.y - 2.3f;
        sw.dial->Update({ sw.pos.x, sw.pos.y + FlipDial::kCenterAbove + bob, sw.pos.z }, groundY, yaw, sw.spin, cooling, now);
    }

    // 確かめ用：スイッチの当たり（球）と、向きのあるレール（裏から見る＝青 / 表から見る＝黄色）
    if ( showDebug_ && ( playing ? showInPlay_ : panelOpen_ ) && context_.railField ) {
        const auto& rails = context_.railField->GetRails();
        for ( int i = 0; i < ( int ) switches_.size(); ++i ) {
            const Switch& sw = switches_[i];
            const Vector3 center { sw.pos.x, sw.pos.y + FlipDial::kCenterAbove, sw.pos.z };
            DebugDraw::GetInstance()->Sphere(center, sw.hitRadius, i == selected_ ? Vector4 { 1.0f, 0.4f, 1.0f, 0.8f } : Vector4 { 0.7f, 0.4f, 1.0f, 0.4f });
        }
        // 切り替えポイント：通ったとみなす範囲（青＝裏へ / 黄＝表へ / 白＝反転）と、立てた棒
        for ( int i = 0; i < ( int ) points_.size(); ++i ) {
            const FlipPoint& point = points_[i];
            if ( point.rail < 0 ) { continue; }
            Vector4 color = point.mode == 2 ? Vector4 { 0.35f, 0.55f, 1.0f, 1.0f } : point.mode == 1 ? Vector4 { 1.0f, 0.85f, 0.25f, 1.0f } : Vector4 { 1.0f, 1.0f, 1.0f, 1.0f };
            if ( i != selectedPoint_ ) { color.w = 0.6f; }
            DebugDraw::GetInstance()->Sphere({ point.pos.x, point.pos.y + 0.5f, point.pos.z }, point.radius, color, 12);
            DebugDraw::GetInstance()->Line(point.pos, { point.pos.x, point.pos.y + 3.0f, point.pos.z }, color);
        }
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

void WorldFlip::Draw(){
    for ( Switch& sw : switches_ ) {
        if ( sw.dial ) { sw.dial->Draw(); }
    }
    if ( eveningSky_ && evening_ > 0.0f ) { eveningSky_->Draw(); } // 表のカメラからは背中側（回り始めてから見えてくる）
}

// 夕方の度合いを、光（夕日の色・少し暗く）・画面の色（うすく橙）・空に。0 になったら元の光へ戻す
void WorldFlip::ApplyEvening(float amount){
    evening_ = Easing::Clamp01(amount);
    Obj3dCommon::DirectionalLightData* light = Obj3dCommon::GetInstance()->GetLightData();
    if ( evening_ <= 0.0f ) {
        RestoreDaylight();
    } else if ( light ) {
        if ( !lightSaved_ ) { savedLight_ = *light; lightSaved_ = true; }
        *light = savedLight_;
        const Vector4& from = savedLight_.lights[0].color;
        light->lights[0].color = { from.x + ( eveningLight_.x - from.x ) * evening_, from.y + ( eveningLight_.y - from.y ) * evening_,
                                   from.z + ( eveningLight_.z - from.z ) * evening_, from.w };
        light->lights[0].intensity = savedLight_.lights[0].intensity * ( 1.0f - 0.12f * evening_ );
        PostEffect* postEffect = PostEffect::GetInstance();
        postEffect->SetEffectActive(PostEffectType::ColorTint, true);
        postEffect->SetColorTint(eveningTint_ * evening_, 1.0f, 0.55f, 0.3f);
    }
    // 空：カメラを真ん中にした大きな筒（地平線より下まで伸ばして、下に隙間を作らない）。
    //   画像の横の真ん中（夕日）が -Z、つまり裏のカメラの正面に来る。表の奥の丘や空の板は筒より手前なので、そのまま見える
    if ( eveningSky_ && context_.camera ) {
        const Vector3 eye = context_.camera->GetWorldPosition();
        eveningSky_->SetTranslation({ eye.x, eye.y + 30.0f, eye.z });
        eveningSky_->SetScale({ 170.0f, 240.0f, 170.0f });
        eveningSky_->Update();
    }
}

void WorldFlip::RestoreDaylight(){
    if ( !lightSaved_ ) { return; }
    if ( Obj3dCommon::DirectionalLightData* light = Obj3dCommon::GetInstance()->GetLightData() ) { *light = savedLight_; }
    lightSaved_ = false;
    PostEffect::GetInstance()->SetColorTint(0.0f, 1.0f, 1.0f, 1.0f);
    PostEffect::GetInstance()->SetEffectActive(PostEffectType::ColorTint, false);
}

// ===== スイッチをレールの上に置く =====
bool WorldFlip::SwitchRailPoint(const Switch& sw, Vector3& outPos, Vector3& outTangent) const{
    if ( !context_.railField ) { return false; }
    const auto& rails = context_.railField->GetRails();
    if ( sw.rail < 0 || sw.rail >= ( int ) rails.size() || rails[sw.rail].nodes.size() < 2 ) { return false; }
    outPos = rails[sw.rail].GetPositionByDistance(sw.dist);
    const Vector3 t = rails[sw.rail].GetTangentByDistance(sw.dist);
    const float len = std::sqrt(t.x * t.x + t.z * t.z);
    outTangent = len > 1e-4f ? Vector3 { t.x / len, 0.0f, t.z / len } : Vector3 { 1.0f, 0.0f, 0.0f };
    return true;
}

// 古い置き方（位置だけ）のスイッチを、真下に一番近いレールへ載せる。載っていれば位置を求め直す
void WorldFlip::UpdateSwitchAnchors(){
    if ( !context_.railField ) { return; }
    const auto& rails = context_.railField->GetRails();
    if ( rails.empty() ) { return; }
    const bool wasClean = savedJson_ == SaveData().dump(); // 求め直し・載せ替えは変更（＊未保存）に数えない
    for ( Switch& sw : switches_ ) {
        if ( sw.rail >= 0 && sw.rail < ( int ) rails.size() && rails[sw.rail].nodes.size() >= 2 ) {
            sw.dist = std::clamp(sw.dist, 0.0f, rails[sw.rail].GetLength());
            const Vector3 p = rails[sw.rail].GetPositionByDistance(sw.dist);
            sw.pos = { p.x, p.y + sw.height, p.z };
            continue;
        }
        if ( sw.rail >= 0 ) { continue; } // レールが消えた：位置はそのまま（パネルに出す）
        int best = -1; float bestDist = 0.0f, bestGap = 3.0f;
        for ( int r = 0; r < ( int ) rails.size(); ++r ) {
            if ( rails[r].nodes.size() < 2 ) { continue; }
            const float d = rails[r].GetClosestDistance(sw.pos);
            const Vector3 p = rails[r].GetPositionByDistance(d);
            const float gap = std::sqrt(( p.x - sw.pos.x ) * ( p.x - sw.pos.x ) + ( p.z - sw.pos.z ) * ( p.z - sw.pos.z ));
            if ( gap < bestGap && p.y <= sw.pos.y ) { best = r; bestDist = d; bestGap = gap; }
        }
        if ( best >= 0 ) {
            sw.rail = best;
            sw.dist = bestDist;
            sw.height = sw.pos.y - rails[best].GetPositionByDistance(bestDist).y;
        }
    }
    if ( wasClean ) { savedJson_ = SaveData().dump(); }
}

// 表のレールから見て、表のカメラ側（手前）の向き。表のカメラは -Z 側から見ている
Vector3 WorldFlip::CameraSideOf(const Vector3& tangent) const{
    Vector3 side { tangent.z, 0.0f, -tangent.x };
    if ( side.z > 0.0f || ( side.z == 0.0f && side.x > 0.0f ) ) { side = { -side.x, 0.0f, -side.z }; }
    return side;
}

// ===== エディタ =====
// スイッチの所からまっすぐ延びる「つなぐ道」と、その先で横へ延びる「裏から見る道」を作る（もうあれば置き直す）。
//   向きは既定で表のカメラ側（手前）＝裏のカメラから見ると奥へ延びて、その先に裏の世界が広がる。
//   つなぐ道の両端は、スイッチの道・裏から見る道の途中に載る（T字）＝分かれ道のキー（奥/手前）で乗り移れる。
//   スイッチの道は「表から見る」、つなぐ道は「変えない」、裏から見る道は「裏から見る」にする
void WorldFlip::BuildLinkRoute(int switchIndex, RailEditor* railEditor){
    if ( !railEditor || switchIndex < 0 || switchIndex >= ( int ) switches_.size() ) { return; }
    Switch& sw = switches_[switchIndex];
    Vector3 base, tangent;
    if ( !SwitchRailPoint(sw, base, tangent) ) { return; }
    const Vector3 side = CameraSideOf(tangent);
    const Vector3 deep = linkTowardCamera_ ? side : Vector3 { -side.x, 0.0f, -side.z }; // 延ばす向き
    const Vector3 joint { base.x + deep.x * linkDepth_, base.y, base.z + deep.z * linkDepth_ };

    std::vector<Vector3> link;
    const int linkSteps = ( std::max )( 2, ( int ) std::ceil(linkDepth_ / 2.0f) );
    for ( int k = 0; k <= linkSteps; ++k ) {
        const float t = ( float ) k / ( float ) linkSteps;
        link.push_back({ base.x + ( joint.x - base.x ) * t, base.y, base.z + ( joint.z - base.z ) * t });
    }
    std::vector<Vector3> back;
    for ( float s = -linkBefore_; s <= linkAfter_ + 0.01f; s += 2.5f ) {
        back.push_back({ joint.x + tangent.x * s, joint.y, joint.z + tangent.z * s });
    }
    if ( back.size() < 2 ) { return; }

    const int count = railEditor->GetRailCount();
    auto place = [&](int& rail, const std::vector<Vector3>& line, bool copyType){
        if ( rail >= 0 && rail < count && rail != sw.rail ) { railEditor->ReplaceRailLine(rail, line); }
        else { rail = railEditor->AddRailLine(line, "反転", sw.rail, copyType); }
    };
    place(sw.backRail, back, true);    // スイッチの道と同じ横のレール
    place(sw.linkRail, link, false);   // 奥へ延びる道は縦（向きは形から決める）
    if ( RailSide(sw.rail) == Side::Both ) { SetRailSide(sw.rail, Side::Front); }
    SetRailSide(sw.linkRail, Side::Both);
    SetRailSide(sw.backRail, Side::Back);
}

// 編集のカメラを、プレイ中と同じ向き・距離で、スイッチ（裏ならその先の裏から見る道）を見る所へ
void WorldFlip::FrameEditorCamera(Camera* camera, int switchIndex, bool back) const{
    if ( !camera || switchIndex < 0 || switchIndex >= ( int ) switches_.size() || !context_.railField ) { return; }
    const Switch& sw = switches_[switchIndex];
    Vector3 target, tangent;
    if ( !SwitchRailPoint(sw, target, tangent) ) { return; }
    const auto& rails = context_.railField->GetRails();
    if ( back && sw.backRail >= 0 && sw.backRail < ( int ) rails.size() && rails[sw.backRail].nodes.size() >= 2 ) {
        target = rails[sw.backRail].GetPositionByDistance(rails[sw.backRail].GetClosestDistance(target));
    }
    const Vector3 side = CameraSideOf(tangent);
    const Vector3 look = back ? side : Vector3 { -side.x, 0.0f, -side.z }; // 表は手前から奥へ、裏は奥から手前へ
    const float distance = 10.0f, height = 3.5f;
    camera->SetTranslation({ target.x - look.x * distance, target.y + height, target.z - look.z * distance });
    camera->SetRotation({ std::atan2(height - 1.0f, distance), std::atan2(look.x, look.z), 0.0f });
}

void WorldFlip::UpdateEditor(const SceneEditContext& context){
    panelOpen_ = context.editor && context.editor->IsPanelVisible(EditorManager::Panel_WorldFlip);
    DrawEditorWindow(context);
    UpdateGameViewTool(context);
    DrawGameViewLabels(context);
}

// Game View の道をクリック：スイッチを置く・レールの向きを塗る。選んでいる間はクリックをレール編集へ渡さない（右クリック・Esc でやめる）
void WorldFlip::UpdateGameViewTool(const SceneEditContext& context){
#ifdef USE_IMGUI
    if ( !panelOpen_ || tool_ == Tool::None || !context.editing || !context.gameView || !context.rails ) { return; }
    const auto& view = context.View();
    if ( !view.hovered || view.gizmoActive ) { return; }
    if ( ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsKeyPressed(ImGuiKey_Escape) ) { tool_ = Tool::None; return; }
    context.editor->SetExternalDragActive(true);
    SceneEditContext::RailPick pick;
    if ( !context.PickRailPoint(0.0f, 60.0f, -1, 0.0f, pick) ) { return; }
    const auto& rails = context.Rails();
    const Vector3 at = rails[pick.rail].GetPositionByDistance(pick.dist);

    Vector4 color { 0.75f, 0.4f, 1.0f, 1.0f };
    const char* what = "スイッチをここへ";
    if ( tool_ == Tool::PlacePoint ) {
        color = pointMode_ == 2 ? Vector4 { 0.4f, 0.6f, 1.0f, 1.0f } : pointMode_ == 1 ? Vector4 { 1.0f, 0.85f, 0.25f, 1.0f } : Vector4 { 1.0f, 1.0f, 1.0f, 1.0f };
        what = pointMode_ == 2 ? "ここを通ると裏から見る" : pointMode_ == 1 ? "ここを通ると表から見る" : "ここを通るたびに反転";
    }
    if ( tool_ == Tool::PaintRail ) {
        color = paintSide_ == 2 ? Vector4 { 0.4f, 0.6f, 1.0f, 1.0f } : paintSide_ == 1 ? Vector4 { 1.0f, 0.85f, 0.25f, 1.0f } : Vector4 { 1.0f, 1.0f, 1.0f, 1.0f };
        what = paintSide_ == 2 ? "このレールは裏から見る" : paintSide_ == 1 ? "このレールは表から見る" : "このレールは向きを変えない";
    }
    DebugDraw::GetInstance()->Sphere(at, 0.3f, color, 10);
    DebugDraw::GetInstance()->Line(at, { at.x, at.y + 2.3f, at.z }, color);
    if ( ImGui::Begin("Game View") ) {
        char text[128];
        snprintf(text, sizeof(text), "クリック：%s（レール%d %.1fm）  右クリック/Esc：やめる", what, pick.rail, pick.dist);
        const ImVec2 mouse { view.mousePos.x + 16.0f, view.mousePos.y + 10.0f };
        const ImVec2 size = ImGui::CalcTextSize(text);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled({ mouse.x - 4.0f, mouse.y - 2.0f }, { mouse.x + size.x + 4.0f, mouse.y + size.y + 2.0f }, IM_COL32(0, 0, 0, 190), 4.0f);
        draw->AddText(mouse, IM_COL32(255, 255, 255, 255), text);
    }
    ImGui::End();
    if ( !ImGui::IsMouseClicked(ImGuiMouseButton_Left) ) { return; }

    if ( tool_ == Tool::SwitchPos ) {
        if ( selected_ < 0 || selected_ >= ( int ) switches_.size() ) {
            Switch sw;
            MakeSwitchObject(sw);
            switches_.push_back(std::move(sw));
            selected_ = ( int ) switches_.size() - 1;
        }
        switches_[selected_].rail = pick.rail;
        switches_[selected_].dist = pick.dist;
    } else if ( tool_ == Tool::PaintRail ) {
        SetRailSide(pick.rail, ( Side ) paintSide_);
    } else if ( tool_ == Tool::PlacePoint ) {
        FlipPoint point;
        point.rail = pick.rail;
        point.dist = pick.dist;
        point.mode = pointMode_;
        point.pos = at;
        points_.push_back(point);
        selectedPoint_ = ( int ) points_.size() - 1;
    }
#else
    ( void ) context;
#endif
}

// Game View の名札：スイッチの番号と、向きのあるレール
void WorldFlip::DrawGameViewLabels(const SceneEditContext& context){
#ifdef USE_IMGUI
    const bool playing = !context.editing;
    if ( !showDebug_ || ( playing ? !showInPlay_ : !panelOpen_ ) || !context.gameView || !context.rails ) { return; }
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
    char text[96];
    for ( int i = 0; i < ( int ) switches_.size(); ++i ) {
        const Switch& sw = switches_[i];
        snprintf(text, sizeof(text), "反転スイッチ %d", i);
        label({ sw.pos.x, sw.pos.y + FlipDial::kCenterAbove + 1.0f, sw.pos.z }, text, i == selected_ ? IM_COL32(150, 60, 200, 230) : IM_COL32(90, 40, 120, 200));
    }
    for ( int i = 0; i < ( int ) points_.size(); ++i ) {
        const FlipPoint& point = points_[i];
        if ( point.rail < 0 ) { continue; }
        snprintf(text, sizeof(text), "%d：%s", i, point.mode == 2 ? "ここから裏" : point.mode == 1 ? "ここから表" : "ここで反転");
        const ImU32 background = point.mode == 2 ? IM_COL32(40, 80, 170, 220) : point.mode == 1 ? IM_COL32(160, 120, 0, 220) : IM_COL32(90, 90, 90, 220);
        label({ point.pos.x, point.pos.y + 3.3f, point.pos.z }, text, background);
    }
    for ( int r = 0; r < ( int ) rails.size(); ++r ) {
        const Side side = RailSide(r);
        if ( side == Side::Both || rails[r].nodes.size() < 2 ) { continue; }
        const Vector3 mid = rails[r].GetPositionByDistance(rails[r].GetLength() * 0.5f);
        snprintf(text, sizeof(text), "%sから見るレール %d", side == Side::Back ? "裏" : "表", r);
        label({ mid.x, mid.y + 0.9f, mid.z }, text, side == Side::Back ? IM_COL32(40, 80, 170, 210) : IM_COL32(160, 120, 0, 210));
    }
    draw->PopClipRect();
    ImGui::End();
#else
    ( void ) context;
#endif
}

// ===== パネル「カメラ反転」 =====
void WorldFlip::DrawEditorWindow(const SceneEditContext& context){
#ifdef USE_IMGUI
    if ( !panelOpen_ ) { return; }
    if ( !ImGui::Begin("カメラ反転 (Camera Flip)") ) { ImGui::End(); return; }
    const bool playing = !context.editing;
    const bool hasSelected = selected_ >= 0 && selected_ < ( int ) switches_.size();
    const ImVec4 kOk { 0.55f, 1.0f, 0.55f, 1.0f }, kTodo { 1.0f, 0.75f, 0.3f, 1.0f };

    if ( ImGui::Button("保存") ) { Save(); }
    ImGui::SameLine();
    if ( ImGui::Button("読み直す") ) { Load(); ResetToFront(); }
    ImGui::SameLine();
    if ( savedJson_ != SaveData().dump() ) { ImGui::TextColored(kTodo, "＊未保存"); }
    else { ImGui::TextDisabled("保存済み"); }
    ImGui::TextDisabled("道を作った時は、マップの保存も");

    ImGui::SeparatorText("しくみ");
    ImGui::TextWrapped("切り替えポイントを置いた所を通ると、カメラが回ります（置いた所でだけ）。スイッチに卵を当てても回ります。");

    // カメラで見る
    ImGui::SeparatorText("カメラで見る（編集中）");
    if ( playing ) { ImGui::BeginDisabled(); }
    if ( ImGui::Button("表のカメラ") ) { previewBack_ = false; FrameEditorCamera(context.camera, selected_, false); }
    ImGui::SameLine();
    if ( ImGui::Button("裏のカメラ") ) {
        previewBack_ = true;
        FrameEditorCamera(context.camera, selected_, true);
        if ( hasSelected ) { previewPivot_ = { switches_[selected_].pos.x, switches_[selected_].pos.y - switches_[selected_].height, switches_[selected_].pos.z }; }
    }
    if ( playing ) { ImGui::EndDisabled(); }

    // Game View で置く・塗る
    ImGui::SeparatorText("Game View の道をクリック");
    if ( playing ) { ImGui::BeginDisabled(); }
    auto toolButton = [&](const char* label, Tool tool){
        const bool active = tool_ == tool;
        if ( active ) { ImGui::PushStyleColor(ImGuiCol_Button, ImVec4 { 0.55f, 0.3f, 0.8f, 1.0f }); }
        if ( ImGui::Button(label) ) { tool_ = active ? Tool::None : tool; }
        if ( active ) { ImGui::PopStyleColor(); }
    };
    toolButton("切り替えポイント", Tool::PlacePoint);
    ImGui::SameLine(); toolButton("スイッチを置く", Tool::SwitchPos);
    toolButton("レールごとに塗る", Tool::PaintRail); // 細いパネルではみ出さないよう2段目
    if ( tool_ == Tool::PlacePoint ) {
        ImGui::RadioButton("裏へ", &pointMode_, 2); ImGui::SameLine();
        ImGui::RadioButton("表へ", &pointMode_, 1); ImGui::SameLine();
        ImGui::RadioButton("反転", &pointMode_, 0);
    }
    if ( tool_ == Tool::PaintRail ) {
        ImGui::RadioButton("裏から見る", &paintSide_, 2); ImGui::SameLine();
        ImGui::RadioButton("表から見る", &paintSide_, 1); ImGui::SameLine();
        ImGui::RadioButton("変えない", &paintSide_, 0);
    }
    ImGui::TextDisabled(tool_ == Tool::None ? "押すと、クリックがこちらへ来ます" : "もう一度押す・右クリック・Esc でやめる");
    if ( playing ) { ImGui::EndDisabled(); }

    // 切り替えポイント
    ImGui::SeparatorText("切り替えポイント");
    static const char* kPointModes[] = { "反転", "表へ", "裏へ" };
    for ( int i = 0; i < ( int ) points_.size(); ++i ) {
        const FlipPoint& point = points_[i];
        char label[96];
        snprintf(label, sizeof(label), "%d：%s　レール%d %.1fm##pt%d", i, kPointModes[point.mode], point.rail, point.dist, i);
        if ( ImGui::Selectable(label, selectedPoint_ == i) ) { selectedPoint_ = i; }
    }
    if ( points_.empty() ) { ImGui::TextDisabled("まだありません（上の「切り替えポイント」を押して道をクリック）"); }
    if ( selectedPoint_ >= 0 && selectedPoint_ < ( int ) points_.size() ) {
        FlipPoint& point = points_[selectedPoint_];
        ImGui::PushID("point");
        ImGui::SetNextItemWidth(110.0f);
        ImGui::Combo("切り替え方", &point.mode, kPointModes, 3);
        ImGui::SetNextItemWidth(160.0f);
        ImGui::DragFloat("レール上の距離(m)", &point.dist, 0.05f, 0.0f, 1000.0f);
        ImGui::SetNextItemWidth(160.0f);
        ImGui::SliderFloat("通ったとみなす半径(m)", &point.radius, 0.5f, 5.0f);
        if ( ImGui::Button("このポイントを消す") ) {
            points_.erase(points_.begin() + selectedPoint_);
            selectedPoint_ = -1;
        }
        ImGui::PopID();
    }

    // スイッチ
    ImGui::SeparatorText("反転スイッチ");
    for ( int i = 0; i < ( int ) switches_.size(); ++i ) {
        const Switch& sw = switches_[i];
        char label[96];
        if ( sw.rail >= 0 ) { snprintf(label, sizeof(label), "%d：レール%d %.1fm##sw%d", i, sw.rail, sw.dist, i); }
        else { snprintf(label, sizeof(label), "%d：レールに載っていない##sw%d", i, i); }
        if ( ImGui::Selectable(label, selected_ == i) ) { selected_ = i; }
    }
    if ( ImGui::Button("消す") && hasSelected ) {
        switches_.erase(switches_.begin() + selected_);
        selected_ = ( std::max )( 0, selected_ - 1 );
    }
    ImGui::SameLine();
    if ( ImGui::Button("新しく置く") ) { selected_ = -1; tool_ = Tool::SwitchPos; } // 次に道をクリックした所へ
    if ( selected_ >= 0 && selected_ < ( int ) switches_.size() ) {
        Switch& sw = switches_[selected_];
        if ( sw.rail >= 0 ) {
            ImGui::SetNextItemWidth(160.0f);
            ImGui::DragFloat("レール上の距離(m)", &sw.dist, 0.05f, 0.0f, 1000.0f);
            ImGui::SetNextItemWidth(160.0f);
            ImGui::SliderFloat("道からの高さ(m)", &sw.height, 0.5f, 6.0f);
        } else {
            ImGui::DragFloat3("位置", &sw.pos.x, 0.05f);
        }
        ImGui::SetNextItemWidth(160.0f);
        ImGui::SliderFloat("卵の当たり半径", &sw.hitRadius, 0.3f, 3.0f);

        // つなぐ道
        ImGui::SeparatorText("つなぐ道（スイッチの所から）");
        int toward = linkTowardCamera_ ? 1 : 0;
        ImGui::RadioButton("裏から見て奥へ", &toward, 1);
        ImGui::SameLine();
        ImGui::RadioButton("表から見て奥へ", &toward, 0);
        linkTowardCamera_ = ( toward == 1 );
        ImGui::SetNextItemWidth(160.0f);
        ImGui::SliderFloat("離す(m)", &linkDepth_, 3.0f, 30.0f);
        ImGui::SetNextItemWidth(160.0f);
        ImGui::SliderFloat("裏の道：前(m)", &linkBefore_, 0.0f, 40.0f);
        ImGui::SetNextItemWidth(160.0f);
        ImGui::SliderFloat("裏の道：後ろ(m)", &linkAfter_, 2.5f, 80.0f);
        const bool canBuild = !playing && context.railEditor && sw.rail >= 0;
        if ( !canBuild ) { ImGui::BeginDisabled(); }
        const bool exists = sw.backRail >= 0 && sw.linkRail >= 0;
        if ( ImGui::Button(exists ? "つなぐ道を置き直す" : "つなぐ道を作る") ) {
            BuildLinkRoute(selected_, context.railEditor);
            previewBack_ = true;
            FrameEditorCamera(context.camera, selected_, true);
            previewPivot_ = { sw.pos.x, sw.pos.y - sw.height, sw.pos.z };
        }
        if ( !canBuild ) { ImGui::EndDisabled(); }
        ImGui::TextDisabled("まっすぐ延びる道（変えない）と、その先の横の道（裏から見る）");
        if ( exists ) { ImGui::TextColored(kOk, "延びる道：レール%d　裏の道：レール%d", sw.linkRail, sw.backRail); }
    }

    ImGui::SeparatorText("ためす");
    ImGui::Text("今：%sのカメラ %s", back_ ? "裏" : "表", phase_ == Phase::None ? "" : "（回り込み中）");
    if ( !playing ) { ImGui::BeginDisabled(); }
    if ( ImGui::Button("反転を試す（プレイ中）") && phase_ == Phase::None ) { BeginTurn(hasSelected, hasSelected ? selected_ : -1); }
    if ( !playing ) { ImGui::EndDisabled(); }
    ImGui::SameLine();
    if ( ImGui::Button("表へ戻す") ) { ResetToFront(); }
    ImGui::Checkbox("名札と線を出す", &showDebug_);
    ImGui::SameLine();
    ImGui::Checkbox("プレイ中も", &showInPlay_);
    ImGui::TextDisabled("青＝裏から見るレール / 黄＝表から見るレール");

    if ( ImGui::TreeNode("裏から見ると夕方") ) {
        ImGui::Checkbox("裏から見ると夕方にする", &eveningOnBack_);
        ImGui::SliderFloat("夕方の強さ", &eveningStrength_, 0.0f, 1.0f);
        ImGui::ColorEdit3("夕日の色", &eveningLight_.x);
        ImGui::SliderFloat("画面に重ねる橙", &eveningTint_, 0.0f, 0.4f);
        ImGui::TextDisabled("編集中は「裏のカメラ」を押すと見られます");
        ImGui::TreePop();
    }
    if ( ImGui::TreeNode("時間・カメラ") ) {
        ImGui::SliderFloat("ヒットストップ(秒)", &hitStop_, 0.0f, 0.6f);
        ImGui::SliderFloat("回る時間(秒)", &flipTime_, 0.4f, 2.0f);
        ImGui::SliderFloat("スイッチが使えない時間(秒)", &cooldownTime_, 0.0f, 4.0f);
        ImGui::SliderFloat("回る途中に広げる割合", &radiusGrow_, 0.0f, 1.0f);
        ImGui::SliderFloat("裏から見る時に隠す背景(恐竜からの距離m)", &occludeGap_, 0.0f, 10.0f);
        ImGui::TreePop();
    }
    if ( ImGui::TreeNode("レールの向き（一覧）") ) {
        static const char* kSideLabels[] = { "変えない", "表から見る", "裏から見る" };
        const int count = context.rails ? ( int ) context.Rails().size() : 0;
        if ( ( int ) railSides_.size() < count ) { railSides_.resize(count, ( int ) Side::Both); }
        if ( ImGui::BeginTable("flipRails", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingFixedFit) ) {
            for ( int i = 0; i < count; ++i ) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("レール %d", i);
                ImGui::TableNextColumn();
                ImGui::PushID(i);
                ImGui::SetNextItemWidth(110.0f);
                ImGui::Combo("##side", &railSides_[i], kSideLabels, 3);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::TreePop();
    }
    ImGui::TextDisabled("%s", FlipPath(stage_).c_str());
    ImGui::End();
#else
    ( void ) context;
#endif
}
