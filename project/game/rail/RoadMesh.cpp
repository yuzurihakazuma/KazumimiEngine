#include "game/rail/RoadMesh.h"

#include "game/rail/road/RoadJunctionDetector.h"
#include "game/rail/road/RoadJunctionPatchBuilder.h"
#include "game/rail/road/RoadSweepBuilder.h"
#include "game/rail/road/RoadClothBuilder.h"
#include "game/rail/road/RoadProfile.h"
#include "engine/rail/SplineRail.h"
#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/graphics/TextureManager.h"
#include "engine/utils/EditorManager.h"
#include "engine/math/VectorMath.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace VectorMath;
using namespace RoadProfile;

// ジョイント（road_joint）を1個置く。railPos はレール高さの位置（道上面+5mm に載せる）
//   静的レール → ベイク先メッシュへ焼き込み（DC削減。動かないので焼いて問題ない）
//   動くレール → Obj3d プールを使い回し（animOffset 追従が必要なため個別のまま）
void RoadMesh::PlaceJointPiece(const std::vector<SplineRail>& rails, int railIdx,
                               const Vector3& railPos, float yaw){
    Model* jointM = ModelManager::GetInstance()->FindModel("roadJoint");
    if ( !jointM ) return;
    // 上面の高さ(0.25)+5mm に載せ、kTopOffset で「上面がレール線ぴったり」の座標系へ合わせる
    const float jointY = railPos.y + kTopH + 0.005f;
    Vector3 p = { railPos.x, jointY + kTopOffset, railPos.z };

    const bool moving = ( railIdx >= 0 && railIdx < ( int ) rails.size() && rails[railIdx].HasMotion() );
    if ( !moving ) {
        // 静的：まとめメッシュへベイク（ジョイントは表示切替があるので道本体とは別メッシュ）
        bakeJoints_.AppendModel(jointM->GetModelData(), p, yaw);
        return;
    }
    slots_.PlaceMovingJoint(jointM, p, yaw, railIdx, rails[railIdx].animOffset);
}

// ジャンクションの各入口に凸を中心へ向けてジョイントを置く（2本溶接は中央に1個）
void RoadMesh::PlaceJoints(const std::vector<SplineRail>& rails, const RoadJunction& junc){
    if ( junc.arms.size() == 2 ) {
        // 2本の溶接：ノード中央に1個。向きは2方向の二等分線
        Vector3 bi = Add(junc.arms[0].dir, junc.arms[1].dir);
        if ( Length(bi) < 0.1f ) { bi = LeftOf(junc.arms[0].dir); } // ほぼ一直線→横向き
        bi = Normalize(bi);
        PlaceJointPiece(rails, junc.followRail, junc.center, std::atan2(bi.x, bi.z));
        return;
    }
    for ( const RoadArm& arm : junc.arms ) {
        // 入口（t_cut位置）に、凸(+Z)を交差点中心向きで置く
        SplineRail::RailFrame f = rails[arm.rail].GetFrameAtDistance(arm.cutS);
        float yaw = std::atan2(-arm.dir.x, -arm.dir.z);
        PlaceJointPiece(rails, junc.followRail, f.position, yaw);
    }
}

// ジャンクションの検出 → 丸広場/コネクタ/パッチのメッシュ生成 → ジョイント配置
//   生成順（＝スロット順）は 丸広場 → ゆるい溶接のコネクタ → 鋭い溶接/T字/十字 のパッチ
void RoadMesh::BuildJunctions(const std::vector<SplineRail>& rails, std::vector<std::vector<RoadCut>>& cuts){
    RoadJunctionDetector detector;
    detector.Detect(rails, cuts);

    const RoadJunctionPatchBuilder patchBuilder(cornerStyle_);
    const RoadSweepBuilder sweepBuilder(warnLength_);

    // 端の丸広場（腕1本のジャンクション）
    for ( const RoadJunction& plaza : detector.Plazas() ) {
        RoadMeshBuilder mesh;
        patchBuilder.Build(rails, plaza, mesh);
        slots_.EmitMesh(mesh.Data(), plaza.followRail);
    }
    // ゆるい角の2本溶接：掃引コネクタ＋中央のジョイント
    for ( const RoadJunctionDetector::Connector& conn : detector.Connectors() ) {
        RoadMeshBuilder mesh;
        sweepBuilder.BuildConnector(conn.nodes, mesh);
        slots_.EmitMesh(mesh.Data(), conn.joint.followRail);
        PlaceJoints(rails, conn.joint);
    }
    // 鋭い溶接/T字/十字：パッチ＋ジョイント
    for ( const RoadJunction& junc : detector.Junctions() ) {
        RoadMeshBuilder mesh;
        patchBuilder.Build(rails, junc, mesh);
        slots_.EmitMesh(mesh.Data(), junc.followRail);
        PlaceJoints(rails, junc);
    }
}

// レールに沿って道を敷き直す
void RoadMesh::Build(const std::vector<SplineRail>& rails, Camera* camera, bool simple){
    // アトラス（GamePlayScene::LoadResources で先読み済み＝ここではキャッシュが返るだけ）
    uint32_t atlasSrv = TextureManager::GetInstance()->Load("resources/road/road_atlas.png").srvIndex;

    slots_.BeginBuild(camera, atlasSrv);
    // 静的ジョイントのまとめメッシュ（容量は使い回す＝clearのみでヒープ再確保しない）
    bakeJoints_.Clear();

    // ジャンクションの検出＋パッチ配置。各レールの「掃引しない区間」も決まる
    std::vector<std::vector<RoadCut>> cuts(rails.size());
    if ( !simple ) { BuildJunctions(rails, cuts); }

    const RoadSweepBuilder sweepBuilder(warnLength_);
    const int n = static_cast<int>( rails.size() );
    for ( int railIdx = 0; railIdx < n; ++railIdx ) {
        if ( !RoadJunctionDetector::IsRoadRail(rails, railIdx) ) continue;
        const SplineRail& rail = rails[railIdx];

        // 自由端（未接続 or 相手が道なしレール）はフタが必要。
        //   ※丸い road_end モデルの配置は廃止：端だけ形が変わって違和感が出るため、
        //     穴の切り口と同じ「平らな暗色フタ」で統一する（RoadSweepBuilder 内で生成。
        //     端がジャンクションや穴に食われている場合は自動でスキップされる）
        bool capFront = false, capBack = false;
        if ( !simple && !rail.isLoop ) {
            capFront = ( rail.frontConnIndex < 0 || !RoadJunctionDetector::IsRoadRail(rails, rail.frontConnIndex) );
            capBack  = ( rail.backConnIndex < 0  || !RoadJunctionDetector::IsRoadRail(rails, rail.backConnIndex) );
        }

        // 掃引メッシュ本体（ジャンクションの切り詰め範囲は張らない）
        RoadMeshBuilder mesh;
        sweepBuilder.Build(rail, cuts[railIdx], simple, capFront, capBack, mesh);
        slots_.EmitMesh(mesh.Data(), railIdx);
    }

    // 静的レールのジョイントを1メッシュにまとめて出す（DC削減。空なら何もしない）
    slots_.EmitMesh(bakeJoints_.Data(), -1, true);

    slots_.EndBuild();

    // 布ロード（布のみ・布＋土台のレール）
    BuildCloth(rails, camera);
}

// ===== 布ロード =====
namespace {
    float ToLinear(float c){ return std::pow(( std::max )( c, 0.0f ), 2.2f); }
}

// 展開の起点から各レールの端までの道のり（端どうしの連結とT字をたどる）
void RoadMesh::ComputePathDistances(const std::vector<SplineRail>& rails){
    const float inf = ( std::numeric_limits<float>::max )();
    const size_t n = rails.size();
    pathFront_.assign(n, inf);
    pathBack_.assign(n, inf);
    if ( unrollRail_ < 0 || unrollRail_ >= ( int ) n ) return;
    const float originLength = rails[unrollRail_].GetLength();
    pathFront_[unrollRail_] = std::clamp(unrollDist_, 0.0f, originLength);
    pathBack_[unrollRail_] = originLength - pathFront_[unrollRail_];
    auto pathAt = [&](int r, float s){
        const float len = rails[r].GetLength();
        const float a = pathFront_[r] == inf ? inf : pathFront_[r] + s;
        const float b = pathBack_[r] == inf ? inf : pathBack_[r] + ( len - s );
        return ( std::min )( a, b );
    };
    for ( int iteration = 0; iteration < 64; ++iteration ) {
        bool changed = false;
        auto relax = [&](float& target, float value){ if ( value < target - 1e-4f ) { target = value; changed = true; } };
        for ( int r = 0; r < ( int ) n; ++r ) {
            const SplineRail& rail = rails[r];
            if ( rail.nodes.size() < 2 ) continue;
            const float len = rail.GetLength();
            // レールの中：始点と終点
            if ( pathFront_[r] != inf ) relax(pathBack_[r], pathFront_[r] + len);
            if ( pathBack_[r] != inf ) relax(pathFront_[r], pathBack_[r] + len);
            // 端どうし
            for ( int end = 0; end < 2; ++end ) {
                const int other = end == 0 ? rail.frontConnIndex : rail.backConnIndex;
                if ( other < 0 || other >= ( int ) n ) continue;
                const bool toFront = end == 0 ? rail.frontConnToFront : rail.backConnToFront;
                float& mine = end == 0 ? pathFront_[r] : pathBack_[r];
                float& theirs = toFront ? pathFront_[other] : pathBack_[other];
                if ( theirs != inf ) relax(mine, theirs);
                if ( mine != inf ) relax(theirs, mine);
            }
            // T字（このレールの途中から別のレールの端が出ている）
            for ( const auto& branch : rail.branchPoints ) {
                const int other = branch.targetRail;
                if ( other < 0 || other >= ( int ) n ) continue;
                const bool otherFront = branch.targetDist < 0.5f * rails[other].GetLength();
                float& theirs = otherFront ? pathFront_[other] : pathBack_[other];
                const float here = pathAt(r, branch.distance);
                if ( here != inf ) relax(theirs, here);
                if ( theirs != inf ) {
                    relax(pathFront_[r], theirs + branch.distance);
                    relax(pathBack_[r], theirs + ( len - branch.distance ));
                }
            }
        }
        if ( !changed ) break;
    }
}

void RoadMesh::ClothRanges(const std::vector<SplineRail>& rails, int railIndex, std::vector<float>& out) const{
    out.clear();
    const float len = rails[railIndex].GetLength();
    if ( unrollFront_ < 0.0f ) { out = { 0.0f, len }; return; }
    const float inf = ( std::numeric_limits<float>::max )();
    if ( railIndex >= ( int ) pathFront_.size() ) return;
    // 道のり min(前から, 後ろから) が front 以下の所だけ
    const float fromFront = pathFront_[railIndex] == inf ? -1.0f : unrollFront_ - pathFront_[railIndex];
    const float fromBack = pathBack_[railIndex] == inf ? -1.0f : unrollFront_ - pathBack_[railIndex];
    float a = std::clamp(fromFront, -1.0f, len);
    float b = std::clamp(len - fromBack, 0.0f, len + 1.0f);
    // 0.05m 刻みにそろえる（毎フレームのわずかな違いで作り直さない）
    a = a < 0.0f ? -1.0f : std::floor(a * 20.0f) / 20.0f;
    b = b > len ? len + 1.0f : std::ceil(b * 20.0f) / 20.0f;
    if ( a >= b ) { out = { 0.0f, len }; return; } // 両側から届いて全部つながった
    if ( a > 0.0f ) { out.push_back(0.0f); out.push_back(a); }
    if ( b < len ) { out.push_back(b); out.push_back(len); }
}

void RoadMesh::BuildCloth(const std::vector<SplineRail>& rails, Camera* camera){
    const size_t n = rails.size();
    clothSlotOfRail_.assign(n, -1);
    clothShown_.assign(n, {});
    clothLift_.assign(n, 0.0f);
    // 重なりの高さ：つながっている隣とは違う段にする（色分け。重なり順 layer が大きい方を上に）
    std::vector<int> color(n, -1);
    for ( size_t r = 0; r < n; ++r ) {
        if ( !rails[r].skin.HasCloth() ) continue;
        bool used[4] = {};
        auto mark = [&](int other){ if ( other >= 0 && other < ( int ) n && color[other] >= 0 ) used[color[other]] = true; };
        mark(rails[r].frontConnIndex);
        mark(rails[r].backConnIndex);
        for ( const auto& branch : rails[r].branchPoints ) mark(branch.targetRail);
        for ( size_t j = 0; j < n; ++j ) {
            for ( const auto& branch : rails[j].branchPoints ) { if ( branch.targetRail == ( int ) r ) mark(( int ) j); }
        }
        int pick = 0;
        while ( pick < 3 && used[pick] ) ++pick;
        color[r] = pick;
        clothLift_[r] = ( float ) rails[r].skin.layer * 0.006f + ( float ) pick * 0.0025f;
    }
    ComputePathDistances(rails);

    const uint32_t stitched = TextureManager::GetInstance()->Load("resources/road/cloth/cloth_felt.png").srvIndex;
    const uint32_t plain = TextureManager::GetInstance()->Load("resources/road/cloth/cloth_felt_plain.png").srvIndex;
    clothSlots_.BeginBuild(camera, stitched);
    const RoadClothBuilder builder;
    for ( size_t r = 0; r < n; ++r ) {
        const SplineRail& rail = rails[r];
        if ( !rail.skin.HasCloth() || !rail.visible || rail.nodes.size() < 2 || rail.GetLength() <= 0.0f ) continue;
        std::vector<float> shown;
        ClothRanges(rails, ( int ) r, shown);
        std::vector<RoadClothBuilder::Range> ranges;
        for ( size_t k = 0; k + 1 < shown.size(); k += 2 ) ranges.push_back({ shown[k], shown[k + 1] });
        RoadMeshBuilder mesh;
        builder.Build(rails, ( int ) r, ranges, clothLift_[r], mesh);
        // 画像は白っぽいフェルト（0.72）なので、色を少し明るく掛けてフェルトの所がちょうど指定の色になるように
        const Vector3& c = rail.skin.color;
        const float gain = 1.0f / 0.72f;
        const Vector4 tint { ToLinear(c.x) * gain, ToLinear(c.y) * gain, ToLinear(c.z) * gain, 1.0f };
        clothSlotOfRail_[r] = ( int ) clothSlots_.EmitMeshSlot(mesh.Data(), ( int ) r, rail.skin.stitch ? stitched : plain, tint);
        clothShown_[r] = shown;
    }
    clothSlots_.EndBuild();
    unrollDirty_ = false;
}

void RoadMesh::SetUnroll(int originRail, float originDist, float front){
    if ( originRail != unrollRail_ || std::abs(originDist - unrollDist_) > 1e-3f ) {
        unrollRail_ = originRail;
        unrollDist_ = originDist;
        pathFront_.clear(); // 次の UpdateCloth で測り直す
    }
    if ( std::abs(front - unrollFront_) > 1e-4f ) {
        unrollFront_ = front;
        unrollDirty_ = true;
    }
}

void RoadMesh::UpdateCloth(const std::vector<SplineRail>& rails){
    if ( !unrollDirty_ || clothSlotOfRail_.size() != rails.size() ) return;
    unrollDirty_ = false;
    if ( pathFront_.size() != rails.size() ) { ComputePathDistances(rails); }
    const RoadClothBuilder builder;
    for ( size_t r = 0; r < rails.size(); ++r ) {
        const int slot = clothSlotOfRail_[r];
        if ( slot < 0 ) continue;
        std::vector<float> shown;
        ClothRanges(rails, ( int ) r, shown);
        if ( shown == clothShown_[r] ) continue; // 変わっていないレールは作り直さない
        std::vector<RoadClothBuilder::Range> ranges;
        for ( size_t k = 0; k + 1 < shown.size(); k += 2 ) ranges.push_back({ shown[k], shown[k + 1] });
        RoadMeshBuilder mesh;
        builder.Build(rails, ( int ) r, ranges, clothLift_[r], mesh);
        clothSlots_.ReplaceMesh(( size_t ) slot, mesh.Data());
        clothShown_[r] = shown;
    }
}

// 毎フレーム：動くレールの animOffset に追従し、カメラ行列を焼き直す
void RoadMesh::Update(const std::vector<SplineRail>& rails){
    // 「後から出現する道」はプレイ中だけ隠す/せり上げる（エディタでは普通に見せて編集できるように）
    const bool playMode = ( EditorManager::GetInstance()->GetMode() == EngineMode::Play );
    slots_.Update(rails, playMode);
    clothSlots_.Update(rails, playMode);
}

void RoadMesh::Draw() const{
    if ( !visible_ ) return;

    // ジョイントは表示モードに従う（0=エディタのみ / 1=常に / 2=非表示）
    bool showJoints = ( jointVisible_ == 1 ) ||
                      ( jointVisible_ == 0 && EditorManager::GetInstance()->GetMode() == EngineMode::Edit );
    slots_.Draw(showJoints);
    clothSlots_.Draw(false); // 布ロード（土台の上・地面の上に敷く布）
}
