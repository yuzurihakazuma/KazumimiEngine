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
    constexpr float kCropStep = 0.5f;      // 四角で切った道を区切る長さ(m)
    constexpr float kMinToken = 0.07f;     // 敵・ゴールの駒の最小の大きさ（小さすぎると見えない）
    constexpr float kTraceTime = 3.0f;     // 布の道をなぞり切るまで(秒)
    constexpr float kTraceHold = 1.5f;     // なぞり終わってから、もう一度なぞるまで(秒)
    constexpr const char* kBaseModel = "ground_block_step"; // 四角いっぱいに敷く草の台
    // 目印の汽車（箱庭の中の m）
    constexpr float kTrackInset = 2.6f;    // 四角のふちから線路の真ん中まで
    constexpr float kTrackGauge = 0.55f;   // 線路の真ん中から左右のレールまで
    constexpr float kTrackBand = 2.0f;     // 線路のまわりで、草花・ブロック・敵をどける幅
    constexpr float kSleeperStep = 1.2f;   // 枕木の間
    constexpr float kTrackLift = 0.14f;    // 枕木の厚み（レールと汽車はこの上）
    constexpr float kTrainSpeed = 6.0f;    // 汽車の速さ(m/秒)

    // 角の丸い四角の輪（x,z。始点と終点が同じ）
    std::vector<Vector3> RoundedLoop(float cx, float cz, float hx, float hz, float radius, float y, float step){
        std::vector<Vector3> points;
        auto line = [&](float ax, float az, float bx, float bz){
            const float length = std::hypot(bx - ax, bz - az);
            const int count = ( std::max )( 1, ( int ) std::ceil(length / step) );
            for ( int i = 0; i < count; ++i ) {
                const float t = ( float ) i / ( float ) count;
                points.push_back({ cx + ax + ( bx - ax ) * t, y, cz + az + ( bz - az ) * t });
            }
        };
        auto arc = [&](float ox, float oz, float from){
            const int count = ( std::max )( 2, ( int ) std::ceil(radius * kPi * 0.5f / step) );
            for ( int i = 0; i < count; ++i ) {
                const float a = from + kPi * 0.5f * ( float ) i / ( float ) count;
                points.push_back({ cx + ox + std::cos(a) * radius, y, cz + oz + std::sin(a) * radius });
            }
        };
        const float ix = hx - radius, iz = hz - radius;
        line(-ix, -hz, ix, -hz);  arc(ix, -iz, -kPi * 0.5f);
        line(hx, -iz, hx, iz);    arc(ix, iz, 0.0f);
        line(ix, hz, -ix, hz);    arc(-ix, iz, kPi * 0.5f);
        line(-hx, iz, -hx, -iz);  arc(-ix, -iz, kPi);
        points.push_back(points.front());
        return points;
    }
    // 角の丸い四角の輪までの距離（x,z）
    float DistanceToLoop(float x, float z, float cx, float cz, float hx, float hz, float radius){
        const float qx = std::fabs(x - cx) - ( hx - radius ), qz = std::fabs(z - cz) - ( hz - radius );
        const float outside = std::hypot(( std::max )( qx, 0.0f ), ( std::max )( qz, 0.0f ));
        return std::fabs(outside + ( std::min )( ( std::max )( qx, qz ), 0.0f ) - radius);
    }

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

bool StageDiorama::Build(const std::string& stage, const Vector3& top, float width, float depth, const Window& window,
                         Camera* camera, uint32_t noiseSrv, uint32_t envSrv){
    Finalize();
    envSrv_ = envSrv;
    std::ifstream file("resources/map/" + stage + ".json");
    if ( !file.is_open() ) { return false; }
    json level;
    try { file >> level; } catch ( ... ) { return false; }
    if ( !level.contains("railLines") ) { return false; }

    // --- レール（ゲームと同じ形・つながり。動くレールは止めたまま）。ブロック・敵の置き場所を求めるのに使う ---
    std::vector<SplineRail> full;
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
        full.push_back(std::move(rail));
    }
    RailConnections::Build(full);
    auto railPoint = [&](int rail, float distance, Vector3& out){
        if ( rail < 0 || rail >= ( int ) full.size() || full[rail].nodes.size() < 2 ) { return false; }
        out = full[rail].GetPositionByDistance(distance);
        return true;
    };

    // --- ブロック（ステージと同じ置き方。斜面は高い側を隣のブロックへ向ける） ---
    const json blocks = level.value("blocks", json::array());
    auto hasBlock = [&](int rail, float dist, int height, float side){
        for ( const json& other : blocks ) {
            if ( other.value("rail", 0) == rail && other.value("level", 0) == height && std::fabs(other.value("side", 0.0f) - side) < 0.01f
                 && std::fabs(other.value("dist", 0.0f) - dist) < 0.5f ) { return true; }
        }
        return false;
    };
    std::vector<BackdropItem> blockItems;
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
        if ( !BlockShape::PoseOnRail(full, block, pos, yaw, &pitch) ) { continue; }
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
        blockItems.push_back(item);
    }
    Vector3 start {};
    const int startRail = level.value("startRailIndex", 0);
    const bool hasStart = startRail >= 0 && startRail < ( int ) full.size() && full[startRail].nodes.size() >= 2;
    if ( hasStart ) { start = full[startRail].GetPositionByDistance(full[startRail].GetDistanceFromT(( float ) level.value("startNodeIndex", 0))); }

    // --- 切り抜く四角（ステージの中の m）。台座と同じ縦横の比 ---
    const float windowDepth = ( std::max )( window.depth, 4.0f );
    const float windowWidth = windowDepth * width / ( std::max )( depth, 0.01f );
    Vector3 lo { 1e9f, 1e9f, 1e9f }, hi { -1e9f, -1e9f, -1e9f };
    for ( const SplineRail& rail : full ) {
        if ( !rail.visible || rail.nodes.size() < 2 ) { continue; }
        for ( const Vector3& p : rail.nodes ) {
            lo = { ( std::min )( lo.x, p.x ), ( std::min )( lo.y, p.y ), ( std::min )( lo.z, p.z ) };
            hi = { ( std::max )( hi.x, p.x ), ( std::max )( hi.y, p.y ), ( std::max )( hi.z, p.z ) };
        }
    }
    if ( lo.x > hi.x ) { return false; }
    float cx = ( lo.x + hi.x ) * 0.5f, cz = ( lo.z + hi.z ) * 0.5f;
    if ( window.hasCenter ) {
        cx = window.centerX;
        cz = window.centerZ;
    } else {
        // 自動：ブロック・敵・道・スタートがいちばん多く入る所（同じなら左＝スタート側）
        struct Weighted { float x, z, w; };
        std::vector<Weighted> points;
        for ( const BackdropItem& item : blockItems ) { points.push_back({ item.pos.x, item.pos.z, 2.0f }); }
        for ( const json& enemy : level.value("enemies", json::array()) ) {
            Vector3 p;
            if ( railPoint(enemy.value("railIndex", -1), enemy.value("distance", 0.0f), p) ) { points.push_back({ p.x, p.z, 4.0f }); }
        }
        for ( const json& coin : level.value("coins", json::array()) ) {
            Vector3 p;
            if ( railPoint(coin.value("rail", -1), coin.value("dist", 0.0f), p) ) { points.push_back({ p.x, p.z, 1.0f }); }
        }
        for ( const SplineRail& rail : full ) {
            if ( !rail.visible || rail.nodes.size() < 2 || rail.skin.style == RailSkin::Hidden ) { continue; }
            for ( float d = 0.0f; d <= rail.GetLength(); d += 1.0f ) {
                const Vector3 p = rail.GetPositionByDistance(d);
                points.push_back({ p.x, p.z, 0.2f });
            }
        }
        if ( hasStart ) { points.push_back({ start.x, start.z, 8.0f }); }
        auto range = [](float a, float b, float size, std::vector<float>& out){
            if ( b - a <= size ) { out.push_back(( a + b ) * 0.5f); return; }
            for ( float c = a + size * 0.5f; c <= b - size * 0.5f + 0.01f; c += 2.0f ) { out.push_back(c); }
        };
        std::vector<float> xs, zs;
        range(lo.x - 2.0f, hi.x + 2.0f, windowWidth, xs);
        range(lo.z - 2.0f, hi.z + 2.0f, windowDepth, zs);
        float best = -1.0f;
        for ( float x : xs ) {
            for ( float z : zs ) {
                float score = 0.0f;
                for ( const Weighted& p : points ) {
                    if ( std::fabs(p.x - x) <= windowWidth * 0.5f && std::fabs(p.z - z) <= windowDepth * 0.5f ) { score += p.w; }
                }
                if ( score > best + 0.01f ) { best = score; cx = x; cz = z; }
            }
        }
    }
    const float x0 = cx - windowWidth * 0.5f, x1 = cx + windowWidth * 0.5f;
    const float z0 = cz - windowDepth * 0.5f, z1 = cz + windowDepth * 0.5f;
    auto inside = [&](float x, float z, float inset){ return x >= x0 + inset && x <= x1 - inset && z >= z0 + inset && z <= z1 - inset; };
    scale_ = width / windowWidth;

    // --- 道：四角の中だけを細かく区切って作り直す（はみ出した所で切れる） ---
    for ( const SplineRail& rail : full ) {
        if ( !rail.visible || rail.nodes.size() < 2 ) { continue; }
        const float length = rail.GetLength();
        const int steps = ( std::max )( 2, ( int ) std::ceil(length / kCropStep) );
        std::vector<Vector3> run;
        std::vector<int> holes;
        bool fromStart = false;
        auto flush = [&](bool toEnd){
            if ( run.size() >= 2 ) {
                SplineRail piece;
                piece.nodes = run;
                piece.lineMode = 1; // 細かく区切ったので直線でつなぐ
                piece.BuildDistanceTable();
                piece.visible = true;
                piece.groundType = rail.groundType;
                piece.roadMode = rail.roadMode;
                piece.skin = rail.skin;
                piece.endPlaza = ( fromStart ? ( rail.endPlaza & 1 ) : 0 ) | ( toEnd ? ( rail.endPlaza & 2 ) : 0 );
                if ( std::find(holes.begin(), holes.end(), 1) != holes.end() ) { piece.nodeHole = holes; }
                piece.AutoDetectType();
                rails_.push_back(std::move(piece));
            }
            run.clear();
            holes.clear();
        };
        for ( int k = 0; k <= steps; ++k ) {
            const float d = length * ( float ) k / ( float ) steps;
            const Vector3 p = rail.GetPositionByDistance(d);
            if ( inside(p.x, p.z, 0.4f) ) {
                if ( run.empty() ) { fromStart = ( k == 0 ); }
                run.push_back(p);
                holes.push_back(rail.IsHoleAtDistance(d) ? 1 : 0);
            } else {
                flush(false);
            }
        }
        flush(true);
    }
    if ( rails_.empty() ) { return false; }
    RailConnections::Build(rails_);

    // --- 地面・木・草花（背景）：四角の外・空・丘・雲・穴の底は外す。地面は四角のふちで切る ---
    backdrop_.LoadStage(stage, noiseSrv);
    float groundBottom = 1e9f, lowestTop = 1e9f;
    for ( int i = ( int ) backdrop_.Items().size() - 1; i >= 0; --i ) {
        BackdropItem item = backdrop_.Items()[i];
        if ( item.removed ) { continue; }
        if ( backdrop_.IsFloor(i) || IsScenery(item.model) ) { backdrop_.RemoveItem(i); continue; }
        Vector3 boxMin, boxMax;
        if ( backdrop_.IsGround(i) && backdrop_.WorldBounds(i, boxMin, boxMax) ) {
            const float cx0 = ( std::max )( boxMin.x, x0 ), cx1 = ( std::min )( boxMax.x, x1 );
            const float cz0 = ( std::max )( boxMin.z, z0 ), cz1 = ( std::min )( boxMax.z, z1 );
            if ( cx1 - cx0 < 0.05f || cz1 - cz0 < 0.05f ) { backdrop_.RemoveItem(i); continue; }
            if ( cx0 > boxMin.x || cx1 < boxMax.x || cz0 > boxMin.z || cz1 < boxMax.z ) {
                // はみ出した分だけ縮めて、切り口を四角のふちへ合わせる
                item.scale.x *= ( cx1 - cx0 ) / ( boxMax.x - boxMin.x );
                item.scale.z *= ( cz1 - cz0 ) / ( boxMax.z - boxMin.z );
                backdrop_.SetItem(i, item);
                if ( backdrop_.WorldBounds(i, boxMin, boxMax) ) {
                    item.pos.x += ( cx0 + cx1 ) * 0.5f - ( boxMin.x + boxMax.x ) * 0.5f;
                    item.pos.z += ( cz0 + cz1 ) * 0.5f - ( boxMin.z + boxMax.z ) * 0.5f;
                    backdrop_.SetItem(i, item);
                    backdrop_.WorldBounds(i, boxMin, boxMax);
                }
            }
            groundBottom = ( std::min )( groundBottom, boxMin.y );
            lowestTop = ( std::min )( lowestTop, boxMax.y );
            continue;
        }
        if ( !inside(item.pos.x, item.pos.z, 0.3f) ) { backdrop_.RemoveItem(i); }
    }
    float railLow = 1e9f;
    for ( const SplineRail& rail : rails_ ) {
        for ( const Vector3& p : rail.nodes ) { railLow = ( std::min )( railLow, p.y ); }
    }
    if ( groundBottom > 1e8f ) { groundBottom = railLow - 1.0f; lowestTop = railLow; }
    // 目印の汽車：四角のふちに沿った線路。地面は道の高さより上を平らにならし、線路のまわりの物はどける
    const bool hasTrain = window.feature == "train";
    const float trackHx = windowWidth * 0.5f - kTrackInset, trackHz = windowDepth * 0.5f - kTrackInset;
    const float trackRadius = ( std::min )( trackHx, trackHz ) * 0.45f;
    float flatTop = -1e9f;
    for ( const SplineRail& rail : rails_ ) {
        for ( const Vector3& p : rail.nodes ) { flatTop = ( std::max )( flatTop, p.y ); }
    }
    auto onTrack = [&](float x, float z){
        return hasTrain && DistanceToLoop(x, z, cx, cz, trackHx, trackHz, trackRadius) < kTrackBand;
    };
    if ( hasTrain ) {
        for ( int i = ( int ) backdrop_.Items().size() - 1; i >= 0; --i ) {
            BackdropItem item = backdrop_.Items()[i];
            if ( item.removed ) { continue; }
            Vector3 boxMin, boxMax;
            if ( backdrop_.IsGround(i) ) {
                if ( backdrop_.WorldBounds(i, boxMin, boxMax) && boxMax.y > flatTop + 0.3f ) {
                    item.pos.y -= boxMax.y - flatTop;
                    backdrop_.SetItem(i, item);
                }
                continue;
            }
            if ( onTrack(item.pos.x, item.pos.z) ) { backdrop_.RemoveItem(i); continue; }
            if ( item.pos.y > flatTop + 0.3f ) {
                item.pos.y = flatTop;
                backdrop_.SetItem(i, item);
            }
        }
    }
    // 四角いっぱいの草の台（地面の無い所・穴も草に見せて、タイルの形にする）
    {
        Vector3 modelMin, modelMax;
        if ( backdrop_.ModelBounds(kBaseModel, modelMin, modelMax) ) {
            const float height = ( std::max )( lowestTop - 0.03f - groundBottom, 0.1f );
            BackdropItem base;
            base.model = kBaseModel;
            base.scale = { windowWidth / ( modelMax.x - modelMin.x ), height / ( modelMax.y - modelMin.y ), windowDepth / ( modelMax.z - modelMin.z ) };
            base.pos = { cx - ( modelMin.x + modelMax.x ) * 0.5f * base.scale.x, groundBottom - modelMin.y * base.scale.y,
                         cz - ( modelMin.z + modelMax.z ) * 0.5f * base.scale.z };
            backdrop_.AddItem(base);
        }
    }
    for ( const BackdropItem& item : blockItems ) {
        if ( inside(item.pos.x, item.pos.z, 0.2f) && !onTrack(item.pos.x, item.pos.z) ) { backdrop_.AddItem(item); }
    }
    if ( hasTrain ) { BuildTrain(cx, cz, trackHx, trackHz, flatTop, camera); }

    // --- 置き場所：四角の真ん中を台座の真ん中へ、地面の底を台座の上面へ ---
    anchor_ = top;
    pivot_ = { cx, groundBottom, cz };
    yaw_ = 0.0f;
    center_ = ToWorld({ cx, railLow, cz });
    boundsMin_ = { top.x - width * 0.5f, top.y, top.z - depth * 0.5f };
    boundsMax_ = { top.x + width * 0.5f, top.y + ( hi.y - groundBottom ) * scale_, top.z + depth * 0.5f };

    // --- 道（布ロード）。ジョイントは出さない（縮めると点にしか見えない） ---
    road_.SetJointVisible(2);
    road_.Build(rails_, camera);

    // --- 敵とゴール（四角の中にある物だけ。ステージと同じ大きさの比で） ---
    for ( const json& enemy : level.value("enemies", json::array()) ) {
        Vector3 local;
        if ( !railPoint(enemy.value("railIndex", -1), enemy.value("distance", 0.0f), local) || !inside(local.x, local.z, 0.3f)
             || onTrack(local.x, local.z) ) { continue; }
        const int type = enemy.value("type", 0);
        const char* model = type == 1 ? "enemyPlant" : type == 2 ? "enemyAir" : "enemyGround";
        const char* fileName = type == 1 ? "enemy_plant.glb" : type == 2 ? "enemy_air.glb" : "enemy_ground.glb";
        Token token;
        token.skinned = SkinnedObj3d::Create(model, "resources/enemy", fileName);
        if ( !token.skinned ) { continue; }
        token.skinned->SetEnvironmentMap(envSrv_);
        token.skinned->LoadClips("resources/enemy", fileName);
        token.skinned->SetClip(type == 2 ? "Fly" : "Idle", true);
        token.local = local;
        if ( type == 2 ) { token.local.y += 1.4f; }
        token.yaw = kPi; // こっち（カメラのある手前）を向く
        token.scale = ( std::max )( scale_, kMinToken ) * enemy.value("scale", 1.0f);
        token.bobPhase = ( float ) tokens_.size() * 0.7f;
        tokens_.push_back(std::move(token));
    }
    const int goalRail = level.value("goalRailIndex", -1);
    if ( goalRail >= 0 && goalRail < ( int ) full.size() && full[goalRail].nodes.size() >= 2 ) {
        const Vector3 goal = full[goalRail].GetPositionByDistance(full[goalRail].GetDistanceFromT(( float ) level.value("goalNodeIndex", 0)));
        if ( inside(goal.x, goal.z, 0.3f) ) {
            Token token;
            token.obj = Obj3d::Create("menu_target");
            token.local = goal;
            token.yaw = kPi;
            token.scale = ( std::max )( scale_, kMinToken ) * 1.2f;
            tokens_.push_back(std::move(token));
        }
    }

    coinCount_ = ( int ) level.value("coins", json::array()).size();
    pathLength_ = 0.0f;
    for ( const SplineRail& rail : rails_ ) { if ( rail.skin.HasCloth() ) { pathLength_ += rail.GetLength(); } }
    pathLength_ = ( std::max )( pathLength_, 10.0f );
    // なぞり始め：スタートに近い道（スタートが無ければ、いちばん左の道の端）
    startRail_ = 0;
    startDist_ = 0.0f;
    float bestStart = 1e9f;
    for ( int i = 0; i < ( int ) rails_.size(); ++i ) {
        if ( hasStart ) {
            const float d = rails_[i].GetClosestDistance(start);
            const Vector3 p = rails_[i].GetPositionByDistance(d);
            const float gap = ( p.x - start.x ) * ( p.x - start.x ) + ( p.z - start.z ) * ( p.z - start.z );
            if ( gap < bestStart ) { bestStart = gap; startRail_ = i; startDist_ = d; }
        } else if ( rails_[i].nodes.front().x < bestStart ) {
            bestStart = rails_[i].nodes.front().x;
            startRail_ = i;
        }
    }
    bounce_ = 1.0f;
    ApplyRoot();
    built_ = true;
    return true;
}

void StageDiorama::Finalize(){
    tokens_.clear();
    train_.Clear();
    trackMesh_.Clear();
    trackRails_.clear();
    road_.Clear();
    backdrop_.Clear();
    rails_.clear();
    built_ = false;
}

// 箱庭の中 → 世界：台座の中心を支点に、縮めて（跳ねの分も）・回して・台座の上へ
DioramaRoot StageDiorama::Root() const{
    DioramaRoot root;
    root.top = anchor_;
    root.pivot = pivot_;
    root.scale = scale_ * bounce_;
    root.yaw = yaw_;
    return root;
}

Vector3 StageDiorama::ToWorld(const Vector3& local) const{ return Root().Apply(local); }

Vector3 StageDiorama::StartPoint() const{
    if ( startRail_ < 0 || startRail_ >= ( int ) rails_.size() ) { return center_; }
    return ToWorld(rails_[startRail_].GetPositionByDistance(startDist_));
}

void StageDiorama::ApplyRoot(){
    const DioramaRoot root = Root();
    const Vector3 t = root.Translation();
    road_.SetRoot(true, root.scale, root.yaw, t);
    trackMesh_.SetRoot(true, root.scale, root.yaw, t);
    backdrop_.SetRoot(root.scale, root.yaw, t);
}

// 線路：角の丸い四角の輪。枕木は段ボールの細い板、2本のレールは灰色の布。汽車はその上を走る
void StageDiorama::BuildTrain(float cx, float cz, float halfWidth, float halfDepth, float height, Camera* camera){
    const float radius = ( std::min )( halfWidth, halfDepth ) * 0.45f;
    SplineRail center;
    center.nodes = RoundedLoop(cx, cz, halfWidth, halfDepth, radius, height + kTrackLift, 0.5f);
    // 台座が回る向きと同じ向きに走らせる（逆向きだと、回るのと打ち消し合って止まって見える）
    std::reverse(center.nodes.begin(), center.nodes.end());
    center.lineMode = 1;
    center.BuildDistanceTable();
    // 枕木
    for ( float d = 0.0f; d < center.GetLength(); d += kSleeperStep ) {
        const Vector3 p = center.GetPositionByDistance(d);
        const Vector3 tangent = center.GetTangentByDistance(d);
        BackdropItem sleeper;
        sleeper.model = "craftLayer";
        sleeper.pos = { p.x, height, p.z };
        sleeper.rot = { 0.0f, std::atan2(tangent.x, tangent.z) * kDeg, 0.0f };
        sleeper.scale = { kTrackGauge * 2.0f + 0.9f, kTrackLift, 0.45f };
        backdrop_.AddItem(sleeper);
    }
    // 2本のレール
    trackRails_.clear();
    for ( float side : { -kTrackGauge, kTrackGauge } ) {
        SplineRail rail;
        rail.nodes = RoundedLoop(cx, cz, halfWidth + side, halfDepth + side, radius + side, height + kTrackLift, 0.5f);
        rail.lineMode = 1;
        rail.BuildDistanceTable();
        rail.AutoDetectType();
        rail.skin.style = RailSkin::Cloth;
        rail.skin.width = 0.32f;
        rail.skin.lift = 0.01f;
        rail.skin.curl = 0.0f;
        rail.skin.stitch = 0;
        rail.skin.color = { 0.62f, 0.62f, 0.66f };
        trackRails_.push_back(std::move(rail));
    }
    RailConnections::Build(trackRails_);
    trackMesh_.SetJointVisible(2);
    trackMesh_.Build(trackRails_, camera);
    train_.Build(center, envSrv_);
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
    // 選ばれている間は、台座ごとゆっくり回る。離れたら正面（1周の切れ目）へなめらかに戻る
    const float turn = 2.0f * kPi;
    if ( selected_ && !spinHold_ ) {
        yaw_ += deltaTime * turn / ( std::max )( spinSeconds_, 1.0f );
        if ( yaw_ > turn * 8.0f ) { yaw_ -= turn * 8.0f; }
    } else {
        // 正面（1周の切れ目）へ。飛び込み中は、構えの間に戻り切るくらい速く
        const float front = std::round(yaw_ / turn) * turn;
        yaw_ += ( front - yaw_ ) * ( std::min )( deltaTime * ( spinHold_ ? 12.0f : 4.0f ), 1.0f );
    }
    ApplyRoot();

    // 布の道をスタートから順に敷き直して、道筋をなぞる（プレビュー中だけ。くり返す）
    if ( previewing ) {
        if ( traceTime_ < 0.0f ) { traceTime_ = 0.0f; }
        traceTime_ += deltaTime;
        const float pathLength = pathLength_;
        const float front = ( std::min )( traceTime_ / kTraceTime, 1.0f ) * pathLength;
        if ( traceTime_ > kTraceTime + kTraceHold ) { traceTime_ = 0.0f; }
        road_.SetUnroll(startRail_, startDist_, ( std::min )( front, pathLength ));
        traceProgress_ = ( std::min )( front / pathLength, 1.0f );
    } else if ( traceTime_ >= 0.0f ) {
        traceTime_ = -1.0f;
        road_.SetUnroll(startRail_, startDist_, -1.0f);
        traceProgress_ = 0.0f;
    }
    road_.UpdateCloth(rails_);
    road_.Update(rails_);
    if ( !trackRails_.empty() ) {
        trackMesh_.UpdateCloth(trackRails_);
        trackMesh_.Update(trackRails_);
    }
    train_.Update(deltaTime, kTrainSpeed, Root());
    backdrop_.Update(camera);

    for ( Token& token : tokens_ ) {
        Vector3 position = ToWorld(token.local);
        const float size = token.scale * bounce_;
        if ( token.skinned ) {
            // プレビュー中だけ動く（ほかのマスの箱庭は止めておく）
            token.skinned->SetPlaybackSpeed(previewing ? 1.0f : 0.0f);
            token.skinned->SetTranslation(position);
            token.skinned->SetScale({ size, size, size });
            token.skinned->SetRotation({ 0.0f, token.yaw + yaw_, 0.0f });
            token.skinned->Update();
        } else if ( token.obj ) {
            // ゴールの駒はゆっくり揺れて目立たせる
            token.bobPhase += deltaTime;
            position.y += 0.02f * std::sin(token.bobPhase * 3.0f);
            token.obj->SetTranslation(position);
            token.obj->SetScale({ size, size, size });
            token.obj->SetRotation({ 0.0f, token.yaw + yaw_ + 0.25f * std::sin(token.bobPhase * 1.4f), 0.0f });
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
    trackMesh_.Draw();
    train_.Draw();
    for ( Token& token : tokens_ ) {
        if ( token.skinned ) { token.skinned->Draw(); }
        else if ( token.obj ) { token.obj->Draw(); }
    }
}
