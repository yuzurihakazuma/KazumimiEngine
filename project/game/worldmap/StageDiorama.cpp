#include "game/worldmap/StageDiorama.h"

#include "engine/3d/obj/Obj3d.h"
#include "engine/3d/obj/SkinnedObj3d.h"
#include "engine/utils/Easing.h"
#include "externals/nlohmann/json.hpp"
#include "game/rail/RailConnections.h"
#include "game/stage/BlockShape.h"

#include <algorithm>
#include <cmath>
#include <fstream>

using json = nlohmann::json;

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kDeg = 180.0f / kPi;
    constexpr float kMargin = 3.0f;        // 道のまわりに残す地面(m)
    constexpr float kEnemyToken = 0.09f;   // 敵の駒の大きさ（本物の 1m の敵 → 9cm）
    constexpr float kGoalToken = 0.11f;    // ゴールの駒
    constexpr float kTraceSpeed = 70.0f;   // 布の道をなぞる速さ（ステージの中の m/秒）
    constexpr float kTraceHold = 1.5f;     // なぞり終わってから、もう一度なぞるまで(秒)

    // ブロックの種類 → モデル（BlockRenderer と同じ並び）
    const char* kBlockModels[] = { "craftSponge", "craftLayer", "craftSlope45", "craftSlope26", "craftSpring",
                                   "craftHatena", "craftCloud", "craftWide", "craftPedestal" };

    template <class T>
    T At(const json& level, const char* key, size_t i, const T& fallback){
        if ( !level.contains(key) || !level[key].is_array() || i >= level[key].size() ) { return fallback; }
        try { return level[key][i].get<T>(); } catch ( ... ) { return fallback; }
    }
    bool IsScenery(const std::string& model){
        return model == "sky_backdrop" || model.rfind("hill_", 0) == 0 || model.rfind("cloud_", 0) == 0;
    }
}

StageDiorama::StageDiorama() = default;
StageDiorama::~StageDiorama() = default;

bool StageDiorama::Build(const std::string& stage, const Vector3& top, float width, float depth, Camera* camera, uint32_t noiseSrv, uint32_t envSrv){
    Finalize();
    envSrv_ = envSrv;
    std::ifstream file("resources/map/" + stage + ".json");
    if ( !file.is_open() ) { return false; }
    json level;
    try { file >> level; } catch ( ... ) { return false; }
    if ( !level.contains("railLines") ) { return false; }

    // --- レール（ゲームと同じ形・つながり。動くレールは止めたまま） ---
    const json& lines = level["railLines"];
    for ( size_t i = 0; i < lines.size(); ++i ) {
        SplineRail rail;
        for ( const json& point : lines[i] ) {
            if ( point.is_array() && point.size() >= 3 ) { rail.nodes.push_back({ point[0].get<float>(), point[1].get<float>(), point[2].get<float>() }); }
        }
        rail.lineMode = At<int>(level, "railLineModes", i, 0);
        rail.BuildDistanceTable();
        rail.visible = At<int>(level, "railVisible", i, 1) != 0;
        rail.groundType = static_cast<SplineRail::GroundType>( At<int>(level, "railGroundTypes", i, 0) );
        rail.roadMode = At<int>(level, "railRoadModes", i, 0);
        rail.endPlaza = At<int>(level, "railEndPlazas", i, 0);
        rail.nodeHole = At<std::vector<int>>(level, "railNodeHoles", i, {});
        const int type = At<int>(level, "railTypes", i, -1);
        if ( type == 0 )      { rail.type = SplineRail::RailType::Horizontal; }
        else if ( type == 1 ) { rail.type = SplineRail::RailType::Vertical; }
        else if ( rail.nodes.size() >= 2 ) { rail.AutoDetectType(); }
        if ( level.contains("railSkins") && i < level["railSkins"].size() ) {
            const json& skin = level["railSkins"][i];
            rail.skin.style = skin.value("style", rail.skin.style);
            rail.skin.width = skin.value("width", rail.skin.width);
            rail.skin.lift = skin.value("lift", rail.skin.lift);
            rail.skin.curl = skin.value("curl", rail.skin.curl);
            rail.skin.stitch = skin.value("stitch", rail.skin.stitch);
            rail.skin.layer = skin.value("layer", rail.skin.layer);
            if ( skin.contains("color") && skin["color"].size() >= 3 ) {
                rail.skin.color = { skin["color"][0].get<float>(), skin["color"][1].get<float>(), skin["color"][2].get<float>() };
            }
        }
        rails_.push_back(std::move(rail));
    }
    RailConnections::Build(rails_);

    // --- 大きさ：見えるレールの範囲（＋まわりの地面）が台座に収まるように ---
    Vector3 lo { 1e9f, 1e9f, 1e9f }, hi { -1e9f, -1e9f, -1e9f };
    for ( const SplineRail& rail : rails_ ) {
        if ( !rail.visible || rail.nodes.size() < 2 ) { continue; }
        for ( const Vector3& p : rail.nodes ) {
            lo = { ( std::min )( lo.x, p.x ), ( std::min )( lo.y, p.y ), ( std::min )( lo.z, p.z ) };
            hi = { ( std::max )( hi.x, p.x ), ( std::max )( hi.y, p.y ), ( std::max )( hi.z, p.z ) };
        }
    }
    if ( lo.x > hi.x ) { return false; }
    lo = { lo.x - kMargin, lo.y, lo.z - kMargin };
    hi = { hi.x + kMargin, hi.y, hi.z + kMargin };
    scale_ = ( std::min )( width / ( hi.x - lo.x ), depth / ( hi.z - lo.z ) );

    // --- 地面・木・草花（背景）：台座からはみ出す物（空・丘・雲・穴の底・範囲の外）は外す ---
    backdrop_.LoadStage(stage, noiseSrv);
    float groundBottom = 1e9f;
    for ( int i = ( int ) backdrop_.Items().size() - 1; i >= 0; --i ) {
        const BackdropItem& item = backdrop_.Items()[i];
        if ( item.removed ) { continue; }
        const bool outside = item.pos.x < lo.x || item.pos.x > hi.x || item.pos.z < lo.z || item.pos.z > hi.z;
        if ( backdrop_.IsFloor(i) || IsScenery(item.model) || outside ) { backdrop_.RemoveItem(i); continue; }
        Vector3 boxMin, boxMax;
        if ( backdrop_.IsGround(i) && backdrop_.WorldBounds(i, boxMin, boxMax) ) { groundBottom = ( std::min )( groundBottom, boxMin.y ); }
    }
    if ( groundBottom > 1e8f ) { groundBottom = lo.y - 1.0f; }

    // --- ブロック（ステージと同じ置き方。斜面は高い側を隣のブロックへ向ける） ---
    const json blocks = level.value("blocks", json::array());
    auto hasBlock = [&](int rail, float dist, int height, float side){
        for ( const json& other : blocks ) {
            if ( other.value("rail", 0) == rail && other.value("level", 0) == height && std::fabs(other.value("side", 0.0f) - side) < 0.01f
                 && std::fabs(other.value("dist", 0.0f) - dist) < 0.5f ) { return true; }
        }
        return false;
    };
    for ( const json& entry : blocks ) {
        PlacedBlock block;
        block.rail = entry.value("rail", 0);
        block.dist = entry.value("dist", 0.0f);
        block.level = entry.value("level", 0);
        block.side = entry.value("side", 0.0f);
        block.type = std::clamp(entry.value("type", 0), 0, BlockShape::kTypeCount - 1);
        if ( BlockShape::IsSlope(block.type) ) {
            block.ascend = hasBlock(block.rail, block.dist + 1.0f, block.level, block.side) ? 1
                         : hasBlock(block.rail, block.dist - 1.0f, block.level, block.side) ? -1 : 1;
        }
        Vector3 pos {}; float yaw = 0.0f, pitch = 0.0f;
        if ( !BlockShape::PoseOnRail(rails_, block, pos, yaw, &pitch) ) { continue; }
        BackdropItem item;
        item.model = kBlockModels[block.type];
        item.scale = { 1.0f, 1.0f, 1.0f };
        if ( block.type == BlockShape::kTypeCloud ) { pos.y += 0.7f; item.scale.y = 0.3f; }
        if ( BlockShape::IsSlope(block.type) ) {
            const float forward = ( float ) block.ascend * BlockShape::FootprintHalf(block.type);
            pos.x += std::sin(yaw) * forward;
            pos.z += std::cos(yaw) * forward;
            pos.y += -std::tan(pitch) * forward;
            if ( block.ascend < 0 ) { yaw += kPi; pitch = -pitch; }
        }
        item.pos = pos;
        item.rot = { pitch * kDeg, yaw * kDeg, 0.0f };
        backdrop_.AddItem(item);
    }

    // --- 置き場所：ステージの真ん中を台座の真ん中へ、地面の底を台座の上面へ ---
    anchor_ = top;
    const Vector3 middle { ( lo.x + hi.x ) * 0.5f, groundBottom, ( lo.z + hi.z ) * 0.5f };
    translate_ = { top.x - middle.x * scale_, top.y - middle.y * scale_, top.z - middle.z * scale_ };
    center_ = ToWorld({ middle.x, lo.y, middle.z });
    boundsMin_ = { top.x - ( hi.x - lo.x ) * 0.5f * scale_, top.y, top.z - ( hi.z - lo.z ) * 0.5f * scale_ };
    boundsMax_ = { top.x + ( hi.x - lo.x ) * 0.5f * scale_, top.y + ( hi.y - groundBottom ) * scale_, top.z + ( hi.z - lo.z ) * 0.5f * scale_ };

    // --- 道（布ロード）。ジョイントは出さない（縮めると点にしか見えない） ---
    road_.SetJointVisible(2);
    road_.Build(rails_, camera);

    // --- 敵とゴールの駒 ---
    for ( const json& enemy : level.value("enemies", json::array()) ) {
        const int railIndex = enemy.value("railIndex", -1);
        if ( railIndex < 0 || railIndex >= ( int ) rails_.size() || rails_[railIndex].nodes.size() < 2 ) { continue; }
        const int type = enemy.value("type", 0);
        const char* model = type == 1 ? "enemyPlant" : type == 2 ? "enemyAir" : "enemyGround";
        const char* fileName = type == 1 ? "enemy_plant.glb" : type == 2 ? "enemy_air.glb" : "enemy_ground.glb";
        Token token;
        token.skinned = SkinnedObj3d::Create(model, "resources/enemy", fileName);
        if ( !token.skinned ) { continue; }
        token.skinned->SetEnvironmentMap(envSrv_);
        token.skinned->LoadClips("resources/enemy", fileName);
        token.skinned->SetClip(type == 2 ? "Fly" : "Idle", true);
        token.local = rails_[railIndex].GetPositionByDistance(enemy.value("distance", 0.0f));
        if ( type == 2 ) { token.local.y += 1.4f; }
        token.yaw = kPi; // こっち（カメラのある手前）を向く
        token.scale = kEnemyToken * enemy.value("scale", 1.0f);
        token.bobPhase = ( float ) tokens_.size() * 0.7f;
        tokens_.push_back(std::move(token));
    }
    // ゴール：決まっていればその地点、無ければいちばん右の道の端
    Vector3 goal {};
    const int goalRail = level.value("goalRailIndex", -1);
    if ( goalRail >= 0 && goalRail < ( int ) rails_.size() && rails_[goalRail].nodes.size() >= 2 ) {
        goal = rails_[goalRail].GetPositionByDistance(rails_[goalRail].GetDistanceFromT(( float ) level.value("goalNodeIndex", 0)));
    } else {
        float best = -1e9f;
        for ( const SplineRail& rail : rails_ ) {
            if ( !rail.visible || rail.nodes.size() < 2 ) { continue; }
            for ( const Vector3& end : { rail.nodes.front(), rail.nodes.back() } ) {
                if ( end.x > best ) { best = end.x; goal = end; }
            }
        }
    }
    {
        Token token;
        token.obj = Obj3d::Create("menu_target");
        token.local = goal;
        token.yaw = kPi;
        token.scale = kGoalToken;
        tokens_.push_back(std::move(token));
    }

    coinCount_ = ( int ) level.value("coins", json::array()).size();
    pathLength_ = 0.0f;
    for ( const SplineRail& rail : rails_ ) { if ( rail.visible && rail.skin.HasCloth() ) { pathLength_ += rail.GetLength(); } }
    pathLength_ = ( std::max )( pathLength_, 30.0f );
    startRail_ = std::clamp(level.value("startRailIndex", 0), 0, ( int ) rails_.size() - 1);
    startDist_ = rails_[startRail_].nodes.size() >= 2 ? rails_[startRail_].GetDistanceFromT(( float ) level.value("startNodeIndex", 0)) : 0.0f;
    bounce_ = 1.0f;
    ApplyRoot();
    built_ = true;
    return true;
}

void StageDiorama::Finalize(){
    tokens_.clear();
    road_.Clear();
    backdrop_.Clear();
    rails_.clear();
    built_ = false;
}

Vector3 StageDiorama::ToWorld(const Vector3& local) const{
    const float s = scale_ * bounce_;
    const Vector3 t { anchor_.x + ( translate_.x - anchor_.x ) * bounce_, anchor_.y + ( translate_.y - anchor_.y ) * bounce_,
                      anchor_.z + ( translate_.z - anchor_.z ) * bounce_ };
    return { local.x * s + t.x, local.y * s + t.y, local.z * s + t.z };
}

void StageDiorama::ApplyRoot(){
    const float s = scale_ * bounce_;
    const Vector3 t { anchor_.x + ( translate_.x - anchor_.x ) * bounce_, anchor_.y + ( translate_.y - anchor_.y ) * bounce_,
                      anchor_.z + ( translate_.z - anchor_.z ) * bounce_ };
    road_.SetRoot(true, s, 0.0f, t);
    backdrop_.SetRoot(s, 0.0f, t);
}

void StageDiorama::SetSelected(bool selected){
    if ( selected == selected_ ) { return; }
    selected_ = selected;
    bounceTime_ = 0.0f;
}

void StageDiorama::Splash(){ splashTime_ = 0.0f; }

void StageDiorama::Update(float deltaTime, bool previewing, const Camera* camera){
    if ( !built_ ) { return; }
    // 跳ね：選んだ時は 1.0 → 1.1 → 1.05、離れたら 1.05 → 1.0
    float base = selected_ ? 1.05f : 1.0f;
    if ( bounceTime_ >= 0.0f ) {
        bounceTime_ += deltaTime;
        if ( selected_ ) {
            const float t = bounceTime_;
            base = t < 0.12f ? Easing::Lerp(1.0f, 1.1f, Easing::EaseOutQuad(t / 0.12f))
                 : t < 0.32f ? Easing::Lerp(1.1f, 1.05f, Easing::EaseInOutCubic(( t - 0.12f ) / 0.2f)) : 1.05f;
        } else {
            base = Easing::Lerp(1.05f, 1.0f, Easing::EaseOutQuad(Easing::Clamp01(bounceTime_ / 0.2f)));
        }
        if ( bounceTime_ > 0.4f ) { bounceTime_ = -1.0f; }
    }
    // 飛び込まれた時：水面のように揺れて収まる
    if ( splashTime_ >= 0.0f ) {
        splashTime_ += deltaTime;
        base *= 1.0f + 0.09f * std::exp(-5.0f * splashTime_) * std::sin(splashTime_ * 22.0f);
        if ( splashTime_ > 1.5f ) { splashTime_ = -1.0f; }
    }
    bounce_ = base;
    ApplyRoot();

    // 布の道をスタートから順に敷き直して、道筋をなぞる（プレビュー中だけ。くり返す）
    if ( previewing ) {
        if ( traceTime_ < 0.0f ) { traceTime_ = 0.0f; }
        traceTime_ += deltaTime;
        const float front = traceTime_ * kTraceSpeed;
        const float pathLength = pathLength_;
        if ( front > pathLength + kTraceSpeed * kTraceHold ) { traceTime_ = 0.0f; }
        road_.SetUnroll(startRail_, startDist_, ( std::min )( front, pathLength ));
        traceProgress_ = ( std::min )( front / pathLength, 1.0f );
    } else if ( traceTime_ >= 0.0f ) {
        traceTime_ = -1.0f;
        road_.SetUnroll(startRail_, startDist_, -1.0f);
        traceProgress_ = 0.0f;
    }
    road_.UpdateCloth(rails_);
    road_.Update(rails_);
    backdrop_.Update(camera);

    for ( Token& token : tokens_ ) {
        Vector3 position = ToWorld(token.local);
        const float size = token.scale * bounce_;
        if ( token.skinned ) {
            // プレビュー中だけ動く（ほかのマスの箱庭は止めておく）
            token.skinned->SetPlaybackSpeed(previewing ? 1.0f : 0.0f);
            token.skinned->SetTranslation(position);
            token.skinned->SetScale({ size, size, size });
            token.skinned->SetRotation({ 0.0f, token.yaw, 0.0f });
            token.skinned->Update();
        } else if ( token.obj ) {
            // ゴールの駒はゆっくり揺れて目立たせる
            token.bobPhase += deltaTime;
            position.y += 0.02f * std::sin(token.bobPhase * 3.0f);
            token.obj->SetTranslation(position);
            token.obj->SetScale({ size, size, size });
            token.obj->SetRotation({ 0.0f, token.yaw + 0.25f * std::sin(token.bobPhase * 1.4f), 0.0f });
            token.obj->Update();
        }
    }
}

void StageDiorama::DrawInstanced(const Camera* camera){
    if ( built_ ) { backdrop_.Draw(camera); }
}

void StageDiorama::DrawOpaque(){
    if ( !built_ ) { return; }
    road_.Draw();
    for ( Token& token : tokens_ ) {
        if ( token.skinned ) { token.skinned->Draw(); }
        else if ( token.obj ) { token.obj->Draw(); }
    }
}
