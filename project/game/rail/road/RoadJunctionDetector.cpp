#include "game/rail/road/RoadJunctionDetector.h"

#include "game/rail/road/RoadProfile.h"
#include "engine/rail/SplineRail.h"
#include "engine/math/VectorMath.h"

#include <algorithm>
#include <cmath>
#include <utility>

using namespace VectorMath;
using namespace RoadProfile;
using namespace RoadJunctionGeom;

namespace {

// --- 検出/t_cut のパラメータ（GUIDE_ジャンクション生成）---
const float kMiterMargin = 0.15f;   // マイター交点から入口までの余白
const float kTCutMax     = 2.0f;    // t_cut の絶対上限。角度が175°に近づくとマイター交点距離が
                                    // 数学的に爆発する（t=W/tan((180°-φ)/2)）ため、巨大パッチが
                                    // 道の下や横にはみ出すのを物理的に止める
const float kGentleDot   = -0.866f; // 2本溶接: dot<=これ(150°以上) → 掃引コネクタ担当
// （旧 kStraightDot は廃止：ほぼ一直線でも折れ目に隙間が出るためコネクタを常に掃引する）
const float kArm         = 1.0f;    // 掃引コネクタが両側を削る長さ
const float kPlazaTCut   = 1.1f;    // 端の丸広場：端から入口までの距離（広場の大きさ）

// レール端のレール距離（atFront=始点 0 / 終点 len）
float EndDistance(const SplineRail& rail, bool atFront){ return atFront ? 0.0f : rail.GetLength(); }
// レール端から内側へ d 進んだレール距離
float FromEnd(const SplineRail& rail, bool atFront, float d){ return atFront ? d : rail.GetLength() - d; }
// レール端のノードから「出ていく」方向（未射影。終点側は接線を反転）
Vector3 EndOutwardTangent(const SplineRail& rail, bool atFront){
    return atFront ? rail.GetTangentByDistance(0.0f)
                   : Multiply(rail.GetTangentByDistance(rail.GetLength()), -1.0f);
}

// 交差検出の間引き用：レール毎のAABB（交差しきい値ぶん膨らませたもの）
struct RailBox { Vector3 mn, mx; bool valid = false; };

bool BoxOverlap(const RailBox& a, const RailBox& b){
    return a.mn.x <= b.mx.x && a.mx.x >= b.mn.x &&
           a.mn.y <= b.mx.y && a.mx.y >= b.mn.y &&
           a.mn.z <= b.mx.z && a.mx.z >= b.mn.z;
}
bool InBox(const RailBox& b, const Vector3& p){
    return p.x >= b.mn.x && p.x <= b.mx.x &&
           p.y >= b.mn.y && p.y <= b.mx.y &&
           p.z >= b.mn.z && p.z <= b.mx.z;
}

} // namespace

bool RoadJunctionDetector::IsRoadRail(const std::vector<SplineRail>& rails, int idx){
    return idx >= 0 && idx < ( int ) rails.size() && rails[idx].visible && rails[idx].roadMode == 0 &&
           rails[idx].nodes.size() >= 2 && rails[idx].GetLength() > 0.0f;
}

// ジャンクション（溶接コーナー/T字/十字）の検出（任意角度対応）
//   検出したジャンクションの t_cut 範囲は cuts に登録し、掃引側が面を張らない
void RoadJunctionDetector::Detect(const std::vector<SplineRail>& rails, std::vector<std::vector<RoadCut>>& cuts){
    plazas_.clear();
    connectors_.clear();
    junctions_.clear();

    DetectEndPlazas(rails, cuts);
    DetectWelds(rails, cuts);
    DetectBranches(rails);
    DetectCrossings(rails);

    // --- t_cut 計算（パッチ生成とジョイント配置は RoadMesh 側）---
    for ( RoadJunction& junc : junctions_ ) {
        ComputeArmCuts(rails, junc, cuts);
    }
}

// --- 端の丸広場（エディタ指定）：「腕1本のジャンクション」としてパッチ生成する ---
//   入口リングはレールの実断面から取られるので継ぎ目ゼロで道と一体化し、
//   外周は360°の円弧ウェッジ＝ヨッシー風の丸い広場になる。道はカットして重ねない
void RoadJunctionDetector::DetectEndPlazas(const std::vector<SplineRail>& rails,
                                           std::vector<std::vector<RoadCut>>& cuts){
    const int n = static_cast<int>( rails.size() );
    for ( int r = 0; r < n; ++r ) {
        if ( !IsRoadRail(rails, r) || rails[r].isLoop ) continue; // ループは端が無い
        float len = rails[r].GetLength();
        for ( int side = 0; side < 2; ++side ) {
            bool atFront = ( side == 0 );
            if ( !( rails[r].endPlaza & ( atFront ? 1 : 2 ) ) ) continue;
            float nodeS = EndDistance(rails[r], atFront);
            Vector3 dir;
            if ( !ToHorizontal(EndOutwardTangent(rails[r], atFront), dir) ) continue;

            RoadJunction junc;
            junc.center = rails[r].GetPositionByDistance(nodeS);
            junc.followRail = r;
            float tCut = std::clamp(kPlazaTCut, kTCutMin, ( std::max )( kTCutMin, len * 0.45f ));
            float cutS = atFront ? tCut : len - tCut;
            junc.arms.push_back({ r, nodeS, cutS, atFront, dir });
            cuts[r].push_back({ ( std::min )( nodeS, cutS ), ( std::max )( nodeS, cutS ) });
            plazas_.push_back(junc);
        }
    }
}

// --- A) 端点溶接（2本の共有ノード）---
//   150°以上のゆるい角は掃引コネクタ / 鋭い角はパッチ
void RoadJunctionDetector::DetectWelds(const std::vector<SplineRail>& rails,
                                       std::vector<std::vector<RoadCut>>& cuts){
    const int n = static_cast<int>( rails.size() );
    for ( int i = 0; i < n; ++i ) {
        if ( !IsRoadRail(rails, i) || rails[i].isLoop ) continue;
        for ( int side = 0; side < 2; ++side ) {
            const bool front = ( side == 0 );
            int conn = front ? rails[i].frontConnIndex : rails[i].backConnIndex;
            if ( conn < i ) continue; // 未接続(-1)と、相手側の走査で処理済みのペアを除外
            if ( !IsRoadRail(rails, conn) || rails[conn].isLoop ) continue;

            const SplineRail& a = rails[i];
            const SplineRail& b = rails[conn];
            bool bFront = front ? a.frontConnToFront : a.backConnToFront;

            // ノードから出ていく方向（水平射影）
            Vector3 aDir, bDir;
            if ( !ToHorizontal(EndOutwardTangent(a, front), aDir) ||
                 !ToHorizontal(EndOutwardTangent(b, bFront), bDir) ) continue;

            float d = Dot(aDir, bDir);
            // ※以前は「ほぼ一直線（±5°）は突き合わせのまま」でスキップしていたが、
            //   判定が水平のみのため平地→スロープの折れ目もスキップされ、断面の傾きが
            //   違う2本の間に隙間/段差が見えていた。緩い継ぎ目も必ずコネクタで滑らかに繋ぐ

            if ( d <= kGentleDot ) {
                // --- ゆるい角（150°以上）：掃引コネクタで連続的に繋ぐ ---
                if ( a.GetLength() < 2.5f || b.GetLength() < 2.5f ) continue;
                float trimA = ( std::min )( kArm, a.GetLength() * 0.4f );
                float trimB = ( std::min )( kArm, b.GetLength() * 0.4f );

                Connector cn;
                cn.nodes = {
                    a.GetPositionByDistance(FromEnd(a, front, trimA * 2.0f)),  // pPre
                    a.GetPositionByDistance(FromEnd(a, front, trimA)),         // p0
                    a.GetPositionByDistance(EndDistance(a, front)),            // pc（ノード）
                    b.GetPositionByDistance(FromEnd(b, bFront, trimB)),        // p1
                    b.GetPositionByDistance(FromEnd(b, bFront, trimB * 2.0f)), // pPost
                };

                cuts[i].push_back(front ? RoadCut { 0.0f, trimA } : RoadCut { a.GetLength() - trimA, a.GetLength() });
                cuts[conn].push_back(bFront ? RoadCut { 0.0f, trimB } : RoadCut { b.GetLength() - trimB, b.GetLength() });

                // ゆるい角にもジョイントは置く（2本溶接＝中央1個）
                cn.joint.center = cn.nodes[2];
                cn.joint.followRail = i;
                cn.joint.arms.push_back({ i, EndDistance(a, front), 0.0f, front, aDir });
                cn.joint.arms.push_back({ conn, EndDistance(b, bFront), 0.0f, bFront, bDir });
                connectors_.push_back(std::move(cn));
                continue;
            }

            // --- 鋭い角（150°未満）：ジャンクションパッチ ---
            RoadJunction junc;
            junc.center = a.GetPositionByDistance(EndDistance(a, front));
            junc.followRail = i;
            junc.arms.push_back({ i, EndDistance(a, front), 0.0f, front, aDir });
            junc.arms.push_back({ conn, EndDistance(b, bFront), 0.0f, bFront, bDir });
            junctions_.push_back(junc);
        }
    }
}

// --- B) T字分岐（branchPoints）：本線2本＋支線1本の3方向パッチ ---
void RoadJunctionDetector::DetectBranches(const std::vector<SplineRail>& rails){
    const int n = static_cast<int>( rails.size() );
    for ( int i = 0; i < n; ++i ) {
        if ( !IsRoadRail(rails, i) ) continue;
        for ( const auto& bp : rails[i].branchPoints ) {
            int j = bp.targetRail;
            if ( !IsRoadRail(rails, j) ) continue;

            Vector3 m1raw = rails[i].GetTangentByDistance(bp.distance);
            bool tFront = bp.targetDist < rails[j].GetLength() * 0.5f;
            Vector3 m1, br;
            if ( !ToHorizontal(m1raw, m1) || !ToHorizontal(EndOutwardTangent(rails[j], tFront), br) ) continue;
            if ( std::abs(Dot(m1, br)) > 0.985f ) continue; // 支線が本線とほぼ平行→パッチが潰れる

            RoadJunction junc;
            junc.center = rails[i].GetPositionByDistance(bp.distance);
            junc.followRail = i;
            junc.arms.push_back({ i, bp.distance, 0.0f, true,  m1 });
            junc.arms.push_back({ i, bp.distance, 0.0f, false, Multiply(m1, -1.0f) });
            junc.arms.push_back({ j, EndDistance(rails[j], tFront), 0.0f, tFront, br });
            junctions_.push_back(junc);
        }
    }
}

// --- C) 本体×本体の交差（乗り換えポイント）：4方向パッチ ---
//   総当たり（レール対 × 0.5m歩き × 最近点計算）は本数×総延長で急激に重くなるため、
//   レール毎のAABB（交差しきい値ぶん膨らませたもの）で二段階に間引く：
//     1. AABB同士が重ならないレール対は丸ごとスキップ
//     2. 歩いている点が相手のAABBの外なら最近点計算をスキップ
void RoadJunctionDetector::DetectCrossings(const std::vector<SplineRail>& rails){
    const int n = static_cast<int>( rails.size() );
    std::vector<RailBox> boxes(n);
    for ( int i = 0; i < n; ++i ) {
        if ( !IsRoadRail(rails, i) || rails[i].HasMotion() ) continue;
        RailBox& b = boxes[i];
        b.mn = { 1e30f, 1e30f, 1e30f };
        b.mx = { -1e30f, -1e30f, -1e30f };
        float len = rails[i].GetLength();
        for ( float s = 0.0f; ; s += 1.0f ) {
            Vector3 p = rails[i].GetPositionByDistance(( std::min )( s, len ));
            b.mn.x = ( std::min )( b.mn.x, p.x ); b.mx.x = ( std::max )( b.mx.x, p.x );
            b.mn.y = ( std::min )( b.mn.y, p.y ); b.mx.y = ( std::max )( b.mx.y, p.y );
            b.mn.z = ( std::min )( b.mn.z, p.z ); b.mx.z = ( std::max )( b.mx.z, p.z );
            if ( s >= len ) break;
        }
        const float kInflate = 1.0f; // 交差しきい値0.9m＋余白
        b.mn.x -= kInflate; b.mn.y -= kInflate; b.mn.z -= kInflate;
        b.mx.x += kInflate; b.mx.y += kInflate; b.mx.z += kInflate;
        b.valid = true;
    }

    for ( int i = 0; i < n; ++i ) {
        if ( !IsRoadRail(rails, i) ) continue;
        for ( int j = i + 1; j < n; ++j ) {
            if ( !IsRoadRail(rails, j) ) continue;
            if ( rails[i].HasMotion() || rails[j].HasMotion() ) continue; // 動くレール同士の交差は追従できない
            if ( !boxes[i].valid || !boxes[j].valid ) continue;
            if ( !BoxOverlap(boxes[i], boxes[j]) ) continue; // 遠いレール対は歩かずスキップ

            float lenI = rails[i].GetLength(), lenJ = rails[j].GetLength();
            float lastPlaced = -1e9f;
            for ( float s = 0.0f; s <= lenI; s += 0.5f ) {
                Vector3 p = rails[i].GetPositionByDistance(s);
                if ( !InBox(boxes[j], p) ) continue; // 相手の範囲外なら最近点計算もしない
                float cd = rails[j].GetClosestDistance(p);
                Vector3 q = rails[j].GetPositionByDistance(cd);
                if ( Length(Subtract(q, p)) > 0.9f ) continue;

                // 端の近くは溶接/分岐の領分（二重配置を防ぐ）
                if ( s < 1.5f || s > lenI - 1.5f || cd < 1.5f || cd > lenJ - 1.5f ) continue;
                if ( s - lastPlaced < 2.0f ) continue; // 同じ交差のクラスタ

                // 交差中心を細かく詰める（±0.6mを0.1刻みで最短距離の点へ）
                float bestS = s, bestD = 1e9f;
                for ( float t = s - 0.6f; t <= s + 0.6f; t += 0.1f ) {
                    if ( t < 0.0f || t > lenI ) continue;
                    Vector3 pp = rails[i].GetPositionByDistance(t);
                    float ccd = rails[j].GetClosestDistance(pp);
                    float dd = Length(Subtract(rails[j].GetPositionByDistance(ccd), pp));
                    if ( dd < bestD ) { bestD = dd; bestS = t; }
                }
                float sC = bestS;
                float cdC = rails[j].GetClosestDistance(rails[i].GetPositionByDistance(sC));

                Vector3 A1, B1;
                if ( !ToHorizontal(rails[i].GetTangentByDistance(sC), A1) ) continue;
                if ( !ToHorizontal(rails[j].GetTangentByDistance(cdC), B1) ) continue;
                if ( std::abs(Dot(A1, B1)) > 0.95f ) continue; // ほぼ平行＝交差にならない

                Vector3 pi = rails[i].GetPositionByDistance(sC);
                Vector3 qj = rails[j].GetPositionByDistance(cdC);

                RoadJunction junc;
                junc.center = { ( pi.x + qj.x ) * 0.5f, ( pi.y + qj.y ) * 0.5f, ( pi.z + qj.z ) * 0.5f };
                junc.followRail = i;
                junc.arms.push_back({ i, sC,  0.0f, true,  A1 });
                junc.arms.push_back({ i, sC,  0.0f, false, Multiply(A1, -1.0f) });
                junc.arms.push_back({ j, cdC, 0.0f, true,  B1 });
                junc.arms.push_back({ j, cdC, 0.0f, false, Multiply(B1, -1.0f) });
                junctions_.push_back(junc);
                lastPlaced = sC;
            }
        }
    }
}

// 1ジャンクションの t_cut を計算し、各レールの切り詰め範囲を登録する
//   隣り合う方向ペア（ウェッジ）ごとに、道の縁線（半幅Wオフセット）の交点から
//   マイター距離を求め、t_cut = max(候補) + 余白（最低 kTCutMin）
void RoadJunctionDetector::ComputeArmCuts(const std::vector<SplineRail>& rails, RoadJunction& junc,
                                          std::vector<std::vector<RoadCut>>& cuts){
    // 角度でソート（ウェッジ＝隣り合うペア）
    std::sort(junc.arms.begin(), junc.arms.end(), [](const RoadArm& a, const RoadArm& b){
        return std::atan2(a.dir.z, a.dir.x) < std::atan2(b.dir.z, b.dir.x);
    });

    const int n = static_cast<int>( junc.arms.size() );
    std::vector<float> tcut(n, kTCutMin);
    for ( int i = 0; i < n; ++i ) {
        int j = ( i + 1 ) % n;
        if ( WedgeAngleDeg(junc.arms[i].dir, junc.arms[j].dir) >= kMiterMaxDeg ) continue; // 直進/円弧は t_cut に影響しない

        float t = 0.0f, s = 0.0f;
        if ( !SolveMiter(junc.arms[i].dir, junc.arms[j].dir, kHalfWidth, t, s) ) continue;
        if ( t > 0.0f ) { tcut[i] = ( std::max )( tcut[i], t + kMiterMargin ); }
        if ( s > 0.0f ) { tcut[j] = ( std::max )( tcut[j], s + kMiterMargin ); }
    }

    for ( int i = 0; i < n; ++i ) {
        RoadArm& arm = junc.arms[i];
        float len = rails[arm.rail].GetLength();
        // レールが短い場合は入り込みすぎない（反対側の端やノードを越えない）＋
        // 絶対上限 kTCutMax で巨大パッチを禁止（ゆるい角度でのマイター爆発対策）
        float avail = arm.forward ? ( len - arm.nodeS ) : arm.nodeS;
        float upper = ( std::min )( kTCutMax, ( std::max )( kTCutMin, avail * 0.45f ) );
        float tCut = std::clamp(tcut[i], kTCutMin, upper);
        arm.cutS = arm.forward ? ( arm.nodeS + tCut ) : ( arm.nodeS - tCut );

        float c0 = ( std::min )( arm.nodeS, arm.cutS );
        float c1 = ( std::max )( arm.nodeS, arm.cutS );
        cuts[arm.rail].push_back({ ( std::max )( 0.0f, c0 ), ( std::min )( len, c1 ) });
    }
}
