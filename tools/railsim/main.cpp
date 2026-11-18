// レールのつなぎ目の調査用（描画なし）。ゲーム本体の SplineRail / RailConnections / 道づくり / Player をそのまま使う。
//   railsim conn rails.txt                         … つなぎ目の一覧（すき間・向きの差）
//   railsim mesh rails.txt out.obj                 … 道（土台・継ぎ板・布）の形を OBJ に書き出す
//   railsim walk rails.txt <rail> <dist> <台本>    … プレイヤーを歩かせて、毎フレームの位置を出す
//        台本の例：  X+:200,Z+:60,X+:300   （世界 +X を 200 フレーム押す → +Z を 60 → …。N:30 は何も押さない）
//   railsim frames rails.txt <rail> <from> <to> <step> … レールの位置・向きを並べる
//   railsim train <台本> [board|roll]             … 列車に乗って降りる（駅で待つ・走っている間は降りられない・空なら戻る。board＝渡し板ぱたん / roll＝ころころ）
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <unordered_map>
#define private public
#include "engine/base/TimeManager.h"
#undef private
#include "game/player/Player.h"
#include "game/stage/BlockSystem.h"
#include "engine/rail/SplineRail.h"
#include "game/rail/RailConnections.h"
#include "game/rail/RailMotion.h"
#include "game/rail/road/RoadJunctionDetector.h"
#include "game/rail/road/RoadJunctionPatchBuilder.h"
#include "game/rail/road/RoadSweepBuilder.h"
#include "game/rail/road/RoadClothBuilder.h"
#include "game/rail/road/RoadMeshBuilder.h"

using namespace KazumimiEngine;


static bool LoadRails(const char* path, std::vector<SplineRail>& rails){
    std::ifstream in(path);
    if ( !in ) { std::printf("cannot open %s\n", path); return false; }
    int count = 0;
    in >> count;
    struct Extra { int type, visible, roadMode, endPlaza, ground; RailSkin skin; std::vector<int> holes; };
    std::vector<Extra> extras(count);
    for ( int i = 0; i < count; ++i ) {
        std::string word;
        int nodeCount = 0, lineMode = 0;
        Extra& e = extras[i];
        in >> word >> nodeCount >> lineMode >> e.type >> e.visible >> e.roadMode >> e.endPlaza >> e.ground;
        in >> word >> e.skin.style >> e.skin.width >> e.skin.lift >> e.skin.curl >> e.skin.layer;
        SplineRail rail;
        in >> word >> rail.EditMotion().motionAmp.x >> rail.EditMotion().motionAmp.y >> rail.EditMotion().motionAmp.z >> rail.EditMotion().motionType >> rail.EditMotion().guideRail;
        for ( int k = 0; k < nodeCount; ++k ) { Vector3 p; in >> p.x >> p.y >> p.z; rail.EditNodes().push_back(p); }
        int holeCount = 0;
        in >> word >> holeCount;
        e.holes.resize(holeCount);
        for ( int k = 0; k < holeCount; ++k ) { in >> e.holes[k]; }
        rail.EditConfig().lineMode = lineMode;
        rail.BuildDistanceTable();
        rails.push_back(rail);
    }
    for ( int i = 0; i < count; ++i ) {
        if ( extras[i].type == 0 ) rails[i].EditConfig().type = SplineRail::RailType::Horizontal;
        else if ( extras[i].type == 1 ) rails[i].EditConfig().type = SplineRail::RailType::Vertical;
        else rails[i].AutoDetectType();
    }
    RailConnections::Build(rails);
    for ( int i = 0; i < count; ++i ) {
        SplineRail& rail = rails[i];
        rail.EditConfig().groundType = ( SplineRail::GroundType ) extras[i].ground;
        rail.EditConfig().nodeHole = extras[i].holes;
        rail.EditConfig().visible = extras[i].visible != 0;
        rail.EditConfig().roadMode = extras[i].roadMode;
        rail.EditConfig().endPlaza = extras[i].endPlaza;
        rail.EditConfig().skin = extras[i].skin;
    }
    return true;
}

static float AngleDeg(const Vector3& a, const Vector3& b){
    const float la = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z), lb = std::sqrt(b.x * b.x + b.y * b.y + b.z * b.z);
    if ( la < 1e-6f || lb < 1e-6f ) return 0.0f;
    float c = ( a.x * b.x + a.y * b.y + a.z * b.z ) / ( la * lb );
    c = c < -1.0f ? -1.0f : ( c > 1.0f ? 1.0f : c );
    return std::acos(c) * 180.0f / 3.14159265f;
}

static void PrintConnections(const std::vector<SplineRail>& rails){
    for ( int a = 0; a < ( int ) rails.size(); ++a ) {
        const SplineRail& ra = rails[a];
        if ( ra.Nodes().size() < 2 ) continue;
        std::printf("R%-2d len %7.3f type %s front->%d(%s) back->%d(%s) branches %d\n", a, ra.GetLength(),
            ra.Config().type == SplineRail::RailType::Horizontal ? "H" : "V", ra.Links().frontConnIndex, ra.Links().frontConnToFront ? "front" : "back",
            ra.Links().backConnIndex, ra.Links().backConnToFront ? "front" : "back", ( int ) ra.Links().branchPoints.size());
        for ( int end = 0; end < 2; ++end ) {
            const bool aFront = end == 0;
            const int b = aFront ? ra.Links().frontConnIndex : ra.Links().backConnIndex;
            if ( b < 0 || b < a ) continue;
            const SplineRail& rb = rails[b];
            const bool bFront = aFront ? ra.Links().frontConnToFront : ra.Links().backConnToFront;
            const float aDist = aFront ? 0.0f : ra.GetLength(), bDist = bFront ? 0.0f : rb.GetLength();
            const Vector3 pa = ra.GetPositionByDistance(aDist), pb = rb.GetPositionByDistance(bDist);
            const SplineRail::RailFrame fa = ra.GetFrameAtDistance(aDist), fb = rb.GetFrameAtDistance(bDist);
            const float sa = aFront ? -1.0f : 1.0f, sb = bFront ? 1.0f : -1.0f;
            Vector3 da = ra.GetTangentByDistance(aDist), db = rb.GetTangentByDistance(bDist);
            da = { da.x * sa, da.y * sa, da.z * sa }; db = { db.x * sb, db.y * sb, db.z * sb };
            const Vector3 rgtA { fa.right.x * sa, fa.right.y * sa, fa.right.z * sa }, rgtB { fb.right.x * sb, fb.right.y * sb, fb.right.z * sb };
            const float gap = std::sqrt(( pa.x - pb.x ) * ( pa.x - pb.x ) + ( pa.y - pb.y ) * ( pa.y - pb.y ) + ( pa.z - pb.z ) * ( pa.z - pb.z ));
            std::printf("   R%d %s <-> R%d %s  at (%.3f %.3f %.3f) gap %.1fmm  tangent %.1f  up %.1f  right %.1f   dirA(%.2f %.2f %.2f) dirB(%.2f %.2f %.2f)\n",
                a, aFront ? "front" : "back", b, bFront ? "front" : "back", pa.x, pa.y, pa.z, gap * 1000.0f,
                AngleDeg(da, db), AngleDeg(fa.up, fb.up), AngleDeg(rgtA, rgtB), da.x, da.y, da.z, db.x, db.y, db.z);
        }
        for ( const auto& bp : ra.Links().branchPoints ) {
            std::printf("   branch at %.3f -> R%d dist %.3f sign %d\n", bp.distance, bp.targetRail, bp.targetDist, bp.zSign);
        }
    }
}

// ---- 道の形を OBJ へ ----
static void WriteObject(FILE* f, const char* name, const Model::ModelData& data, int& vertexBase){
    if ( data.vertices.empty() || data.indices.empty() ) return;
    std::fprintf(f, "o %s\n", name);
    for ( const auto& v : data.vertices ) { std::fprintf(f, "v %.5f %.5f %.5f\n", v.position.x, v.position.y, v.position.z); }
    for ( size_t i = 0; i + 2 < data.indices.size(); i += 3 ) {
        std::fprintf(f, "f %d %d %d\n", vertexBase + ( int ) data.indices[i] + 1, vertexBase + ( int ) data.indices[i + 1] + 1,
            vertexBase + ( int ) data.indices[i + 2] + 1);
    }
    vertexBase += ( int ) data.vertices.size();
}

static void DumpMesh(const std::vector<SplineRail>& rails, const char* path){
    FILE* f = nullptr;
    fopen_s(&f, path, "w");
    if ( !f ) { std::printf("cannot write %s\n", path); return; }
    int base = 0;
    char name[64];
    const int n = ( int ) rails.size();
    std::vector<std::vector<RoadCut>> cuts(n);
    RoadJunctionDetector detector;
    detector.Detect(rails, cuts);
    const RoadJunctionPatchBuilder patchBuilder(0);
    const RoadSweepBuilder sweepBuilder(1.0f);
    int index = 0;
    for ( const RoadJunction& plaza : detector.Plazas() ) {
        RoadMeshBuilder mesh; patchBuilder.Build(rails, plaza, mesh);
        std::snprintf(name, sizeof(name), "plaza%d_r%d", index++, plaza.followRail); WriteObject(f, name, mesh.Data(), base);
    }
    index = 0;
    for ( const auto& conn : detector.Connectors() ) {
        RoadMeshBuilder mesh; sweepBuilder.BuildConnector(conn.nodes, conn.keepFrom, conn.keepTo, mesh);
        std::snprintf(name, sizeof(name), "conn%d_r%d", index++, conn.joint.followRail); WriteObject(f, name, mesh.Data(), base);
        std::printf("connector R%d: center (%.3f %.3f %.3f)\n", conn.joint.followRail, conn.joint.center.x, conn.joint.center.y, conn.joint.center.z);
    }
    index = 0;
    for ( const RoadJunction& junc : detector.Junctions() ) {
        RoadMeshBuilder mesh; patchBuilder.Build(rails, junc, mesh);
        std::snprintf(name, sizeof(name), "patch%d_r%d", index++, junc.followRail); WriteObject(f, name, mesh.Data(), base);
        std::printf("patch R%d: center (%.3f %.3f %.3f) arms", junc.followRail, junc.center.x, junc.center.y, junc.center.z);
        for ( const RoadArm& arm : junc.arms ) { std::printf("  [R%d node %.2f cut %.2f]", arm.rail, arm.nodeS, arm.cutS); }
        std::printf("\n");
    }
    for ( int r = 0; r < n; ++r ) {
        if ( !RoadJunctionDetector::IsRoadRail(rails, r) ) continue;
        const SplineRail& rail = rails[r];
        bool capFront = false, capBack = false;
        if ( !rail.Links().isLoop ) {
            capFront = ( rail.Links().frontConnIndex < 0 || !RoadJunctionDetector::IsRoadRail(rails, rail.Links().frontConnIndex) );
            capBack = ( rail.Links().backConnIndex < 0 || !RoadJunctionDetector::IsRoadRail(rails, rail.Links().backConnIndex) );
        }
        RoadMeshBuilder mesh;
        sweepBuilder.Build(rail, cuts[r], false, capFront, capBack, mesh, detector.EndExtension(r, true), detector.EndExtension(r, false));
        std::snprintf(name, sizeof(name), "base_r%d", r); WriteObject(f, name, mesh.Data(), base);
    }
    // 布（RoadMesh::BuildCloth と同じ重なりの決め方）
    std::vector<int> color(n, -1);
    std::vector<float> lift(n, 0.0f);
    for ( int r = 0; r < n; ++r ) {
        if ( !rails[r].Config().skin.HasCloth() ) continue;
        bool used[4] = {};
        auto mark = [&](int other){ if ( other >= 0 && other < n && color[other] >= 0 ) used[color[other]] = true; };
        mark(rails[r].Links().frontConnIndex);
        mark(rails[r].Links().backConnIndex);
        for ( const auto& branch : rails[r].Links().branchPoints ) mark(branch.targetRail);
        for ( int j = 0; j < n; ++j ) { for ( const auto& branch : rails[j].Links().branchPoints ) { if ( branch.targetRail == r ) mark(j); } }
        int pick = 0;
        while ( pick < 3 && used[pick] ) ++pick;
        color[r] = pick;
        lift[r] = ( float ) rails[r].Config().skin.layer * 0.006f + ( float ) pick * 0.0025f;
    }
    const RoadClothBuilder clothBuilder;
    for ( int r = 0; r < n; ++r ) {
        const SplineRail& rail = rails[r];
        if ( !rail.Config().skin.HasCloth() || !rail.Config().visible || rail.Nodes().size() < 2 || rail.GetLength() <= 0.0f ) continue;
        RoadMeshBuilder mesh;
        clothBuilder.Build(rails, r, { { 0.0f, rail.GetLength() } }, lift[r], mesh);
        std::snprintf(name, sizeof(name), "cloth_r%d", r); WriteObject(f, name, mesh.Data(), base);
    }
    std::fclose(f);
    std::printf("wrote %s (%d vertices)\n", path, base);
}

static void Walk(std::vector<SplineRail>& rails, int rail, float dist, const std::string& script){
    BlockSystem blocks;
    blocks.Sync({}, &rails);
    Player player;
    player.SetBlocks(&blocks);
    player.SetSpawn(rail, dist);
    player.SetCameraYaw(0.0f);
    player.Initialize();
    Time::GetInstance()->deltaTime_ = 1.0f / 60.0f;
    std::stringstream ss(script);
    std::string part;
    int frame = 0;
    Vector3 prev = rails[rail].GetPositionByDistance(dist);
    std::printf("frame,rail,dist,x,y,z,speed,vx,vy,vz,yawDeg,keys\n");
    while ( std::getline(ss, part, ',') ) {
        const size_t colon = part.find(':');
        const std::string keys = part.substr(0, colon);
        const int frames = std::atoi(part.substr(colon + 1).c_str());
        PlayerInput::AutoPilot pilot;
        if ( keys == "X+" ) pilot.moveX = 1.0f;
        if ( keys == "X-" ) pilot.moveX = -1.0f;
        if ( keys == "Z+" ) pilot.moveZ = 1.0f;
        if ( keys == "Z-" ) pilot.moveZ = -1.0f;
        for ( int i = 0; i < frames; ++i, ++frame ) {
            // 乗り換えキーは「押した瞬間」だけ（横レールの上で Z を押した時など）
            PlayerInput::AutoPilot now = pilot;
            if ( i == 0 ) { now.switchX = ( int ) pilot.moveX; now.switchZ = ( int ) pilot.moveZ; }
            player.SetAutoPilot(true, now);
            player.Update(rails);
            const Vector3 p = player.GetPosition();
            const Vector3 v { ( p.x - prev.x ) * 60.0f, ( p.y - prev.y ) * 60.0f, ( p.z - prev.z ) * 60.0f };
            const Player::DebugStep& step = player.GetDebugStep();
            std::printf("%d,%d,%.4f,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%.3f,%.1f,%s\n", frame, step.rail, step.dist, p.x, p.y, p.z,
                std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z), v.x, v.y, v.z, player.GetRotation().y * 180.0f / 3.14159265f, keys.c_str());
            prev = p;
        }
    }
}

// 列車の確かめ：駅 A の道（縦）→ 台車（横・6m）に乗る → 発車 → 走っている間に降りようとする → 駅 B で降りる → 空の列車が戻る。
//   線路＝まっすぐ 40m。駅の道は、止まった台車の真ん中の線まで重ねてある。台本は walk と同じ（J:n＝跳ぶ）
//   boardKind：駅 A の道を台車の 2m 手前で止め、そこから台車までを渡し板（4 番）にする（1＝ぱたん / 2＝ころころ。台本の E:1＝卵が当たる）
static void Train(const std::string& script, int boardKind){
    const bool withBoard = boardKind != 0;
    auto line = [](std::initializer_list<Vector3> points){ SplineRail rail; rail.EditNodes() = points; rail.BuildDistanceTable(); return rail; };
    std::vector<SplineRail> rails;
    if ( withBoard ) { rails.push_back(line({ { 4.0f, 1.0f, -6.0f }, { 4.0f, 1.0f, -4.0f }, { 4.0f, 1.0f, -2.0f } })); } // 0：駅 A の道（板の手前まで）
    else             { rails.push_back(line({ { 4.0f, 1.0f, -6.0f }, { 4.0f, 1.0f, -3.0f }, { 4.0f, 1.0f, 0.0f } })); }  // 0：駅 A の道
    rails.push_back(line({ { 1.0f, 1.0f, 0.0f }, { 4.0f, 1.0f, 0.0f }, { 7.0f, 1.0f, 0.0f } }));         // 1：台車
    rails.push_back(line({ { 0.0f, 1.0f, 0.0f }, { 20.0f, 1.0f, 0.0f }, { 40.0f, 1.0f, 0.0f } }));       // 2：線路
    rails.push_back(line({ { 33.3f, 1.0f, 0.0f }, { 33.3f, 1.0f, 3.0f }, { 33.3f, 1.0f, 6.0f } }));      // 3：駅 B の道
    rails.push_back(line({ { 4.0f, 1.0f, -2.0f }, { 4.0f, 1.0f, -1.0f }, { 4.0f, 1.0f, 0.0f } }));       // 4：渡し板（withBoard の時だけ使う）
    rails[0].EditConfig().type = rails[3].EditConfig().type = rails[4].EditConfig().type = SplineRail::RailType::Vertical;
    if ( withBoard ) { rails[4].EditBoard().boardKind = boardKind; rails[4].EditBoard().boardTrain = boardKind == 1 ? 1 : -1; } else { rails[4].EditNodes().clear(); rails[4].BuildDistanceTable(); }
    rails[1].EditConfig().type = rails[2].EditConfig().type = SplineRail::RailType::Horizontal;
    SplineRail& car = rails[1];
    car.EditMotion().motionType = 3; car.EditMotion().guideRail = 2; car.EditMotion().motionPeriod = 3.0f;
    car.EditTrain().train = 1; car.EditTrain().trainDepart = 0.5f; car.EditTrain().trainReturn = 2.0f; car.EditTrain().trainEngine = 1; car.EditTrain().trainSpeed = 12.0f;
    car.EditMotion().animPivot = car.GetPositionByDistance(car.GetLength() * 0.5f);
    car.EditMotion().restYaw = std::atan2(1.0f, 0.0f);
    rails[2].EditConfig().visible = 0; rails[2].EditConfig().roadMode = 1;
    rails[2].EditTrain().isTrack = true;
    RailMotion::SetupTrain(car, rails);
    RailMotion::SetupBoard(rails[4], rails);
    car.EditConfig().roadMode = 1;
    RailConnections::Build(rails);
    RailMotion::SetupRollBoard(rails[4]);
    RailMotion::Reset(rails);
    std::printf("home %d  station x %.2f\n", car.Train().trainHome, car.Motion().animPivot.x + car.Motion().animOffset.x);

    BlockSystem blocks;
    blocks.Sync({}, &rails);
    Player player;
    player.SetBlocks(&blocks);
    player.SetSpawn(0, 1.0f);
    player.SetCameraYaw(0.0f);
    player.Initialize();
    const float dt = 1.0f / 60.0f;
    Time::GetInstance()->deltaTime_ = dt;
    std::stringstream ss(script);
    std::string part;
    int frame = 0;
    const char* states[] = { "Wait", "Depart", "Run" };
    std::printf("frame,keys,playerRail,dist,x,y,z,grounded,train,carX,board\n");
    while ( std::getline(ss, part, ',') ) {
        const size_t colon = part.find(':');
        const std::string keys = part.substr(0, colon);
        const int frames = std::atoi(part.substr(colon + 1).c_str());
        PlayerInput::AutoPilot pilot;
        if ( keys == "X+" ) pilot.moveX = 1.0f;
        if ( keys == "X-" ) pilot.moveX = -1.0f;
        if ( keys == "Z+" ) pilot.moveZ = 1.0f;
        if ( keys == "Z-" ) pilot.moveZ = -1.0f;
        for ( int i = 0; i < frames; ++i, ++frame ) {
            PlayerInput::AutoPilot now = pilot;
            if ( i == 0 ) { now.switchX = ( int ) pilot.moveX; now.switchZ = ( int ) pilot.moveZ; }
            if ( keys == "J" ) { now.jumpPressed = ( i == 0 ); now.jumpHeld = ( i < 10 ); }
            if ( keys == "E" && i == 0 ) { rails[4].EditBoard().boardTriggered = true; } // 卵が巻いたマットに当たった
            const int riding = player.IsGrounded() ? player.GetCurrentRail() : -1;
            RailMotion::Advance(rails, dt, riding, player.GetCurrentRail());
            player.SetAutoPilot(true, now);
            player.Update(rails);
            const Vector3 p = player.GetPosition();
            const bool print = ( i == 0 || i == frames - 1 || frame % 15 == 0 );
            if ( print ) {
                std::printf("%d,%s,%d,%.2f,%.2f,%.2f,%.2f,%d,%s,%.2f,%.2f\n", frame, keys.c_str(), player.GetCurrentRail(),
                    player.GetDebugStep().dist, p.x, p.y, p.z, player.IsGrounded() ? 1 : 0, states[( int ) car.Train().trainState],
                    car.Motion().animPivot.x + car.Motion().animOffset.x, rails[4].Board().boardOpen);
            }
        }
    }
}

int main(int argc, char** argv){
    if ( argc >= 3 && std::string(argv[1]) == "train" ) { Train(argv[2], argc >= 4 ? ( std::string(argv[3]) == "board" ? 1 : std::string(argv[3]) == "roll" ? 2 : 0 ) : 0); return 0; }
    if ( argc < 3 ) { std::printf("usage: railsim conn|mesh|walk|frames rails.txt ... / railsim train <script>\n"); return 1; }
    const std::string mode = argv[1];
    std::vector<SplineRail> rails;
    if ( !LoadRails(argv[2], rails) ) return 1;
    if ( mode == "conn" ) { PrintConnections(rails); }
    else if ( mode == "mesh" && argc >= 4 ) { DumpMesh(rails, argv[3]); }
    else if ( mode == "walk" && argc >= 6 ) { Walk(rails, std::atoi(argv[3]), ( float ) std::atof(argv[4]), argv[5]); }
    else if ( mode == "frames" && argc >= 7 ) {
        const SplineRail& rail = rails[std::atoi(argv[3])];
        const float from = ( float ) std::atof(argv[4]), to = ( float ) std::atof(argv[5]), step = ( float ) std::atof(argv[6]);
        std::printf("len %.4f\n", rail.GetLength());
        for ( float s = from; s <= to + 1e-4f; s += step ) {
            const float d = s < 0.0f ? rail.GetLength() + s : s;
            const SplineRail::RailFrame fr = rail.GetFrameAtDistance(d);
            const Vector3 p = rail.GetPositionByDistance(d);
            std::printf("s %7.3f pos (%.3f %.3f %.3f) tan (%.3f %.3f %.3f) right (%.3f %.3f %.3f) up (%.3f %.3f %.3f)\n", d, p.x, p.y, p.z,
                fr.tangent.x, fr.tangent.y, fr.tangent.z, fr.right.x, fr.right.y, fr.right.z, fr.up.x, fr.up.y, fr.up.z);
        }
    }
    return 0;
}
