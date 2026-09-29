#include "game/rail/RoadMesh.h"

#include "game/rail/road/RoadJunctionDetector.h"
#include "game/rail/road/RoadJunctionPatchBuilder.h"
#include "game/rail/road/RoadSweepBuilder.h"
#include "game/rail/road/RoadProfile.h"
#include "engine/rail/SplineRail.h"
#include "engine/3d/model/Model.h"
#include "engine/3d/model/ModelManager.h"
#include "engine/graphics/TextureManager.h"
#include "engine/utils/EditorManager.h"
#include "engine/math/VectorMath.h"

#include <cmath>

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
}

// 毎フレーム：動くレールの animOffset に追従し、カメラ行列を焼き直す
void RoadMesh::Update(const std::vector<SplineRail>& rails){
    // 「後から出現する道」はプレイ中だけ隠す/せり上げる（エディタでは普通に見せて編集できるように）
    const bool playMode = ( EditorManager::GetInstance()->GetMode() == EngineMode::Play );
    slots_.Update(rails, playMode);
}

void RoadMesh::Draw() const{
    if ( !visible_ ) return;

    // ジョイントは表示モードに従う（0=エディタのみ / 1=常に / 2=非表示）
    bool showJoints = ( jointVisible_ == 1 ) ||
                      ( jointVisible_ == 0 && EditorManager::GetInstance()->GetMode() == EngineMode::Edit );
    slots_.Draw(showJoints);
}
