#include "game/rail/road/RoadSweepBuilder.h"

#include "game/rail/road/RoadMeshBuilder.h"
#include "game/rail/road/RoadProfile.h"
#include "engine/math/VectorMath.h"

#include <algorithm>
#include <cmath>

using namespace VectorMath;
using namespace RoadProfile;

namespace {

// --- 掃引の生成パラメータ（設計書 §4 の確定値）---
const float kWalkStep   = 0.05f;                            // サンプリング歩幅(m)
const float kMinRingGap = 0.06f;                            // リング最小間隔(m)
const float kMaxRingGap = 1.00f;                            // リング最大間隔(m)
const float kCurveAngle = 3.0f * kPi / 180.0f;              // リングを置く累積角(3°)
const float kSlopeIn    = 0.45f;                            // 坂UVに入る |tangent.y|（ヒステリシス上側）
const float kSlopeOut   = 0.35f;                            // 坂UVから抜ける（下側）
const float kSimpleStep = 1.0f;                             // 簡易ビルド（ドラッグ中）のリング間隔(m)

} // namespace

// 距離 s が危険帯（穴の手前後 warnLength_）か
bool RoadSweepBuilder::DangerAt(const Holes& holes, float s) const{
    for ( const auto& h : holes ) {
        if ( ( s >= h.d0 - warnLength_ && s <= h.d0 + 1e-4f ) ||
             ( s >= h.d1 - 1e-4f && s <= h.d1 + warnLength_ ) ) return true;
    }
    return false;
}

// 簡易ビルド：固定1m刻みの素のリボン（坂UV/穴/溶接なし）
std::vector<RoadSweepBuilder::Ring> RoadSweepBuilder::PlaceSimpleRings(float len){
    std::vector<Ring> rings;
    for ( float s = 0.0f; s < len; s += kSimpleStep ) { rings.push_back({ s, false, false }); }
    rings.push_back({ len, false, false });
    return rings;
}

// 曲率適応サンプリング＋坂UVのヒステリシス
std::vector<RoadSweepBuilder::Ring> RoadSweepBuilder::PlaceAdaptiveRings(const SplineRail& rail, float len,
                                                                         const Holes& holes) const{
    std::vector<Ring> rings;
    SplineRail::RailFrame f0 = rail.GetFrameAtDistance(0.0f);
    bool slope = std::abs(f0.tangent.y) > kSlopeIn;
    rings.push_back({ 0.0f, slope, DangerAt(holes, 0.0f) });

    Vector3 prevTan = f0.tangent;
    float sinceLast = 0.0f, accumAngle = 0.0f;

    for ( float s = kWalkStep; s < len; s += kWalkStep ) {
        SplineRail::RailFrame f = rail.GetFrameAtDistance(s);
        float c = std::clamp(Dot(prevTan, f.tangent), -1.0f, 1.0f);
        accumAngle += std::acos(c);
        prevTan = f.tangent;
        sinceLast += kWalkStep;

        // 坂UVの切替（入り0.45 / 抜け0.35 のヒステリシスでチラつかない）
        float ty = std::abs(f.tangent.y);
        bool newSlope = slope ? ( ty > kSlopeOut ) : ( ty > kSlopeIn );
        if ( newSlope != slope ) {
            rings.push_back({ s, slope, DangerAt(holes, s) });    // 境目は同位置の二重リング（vだけ切替）
            rings.push_back({ s, newSlope, DangerAt(holes, s) });
            slope = newSlope;
            sinceLast = 0.0f;
            accumAngle = 0.0f;
            continue;
        }

        if ( ( accumAngle > kCurveAngle && sinceLast >= kMinRingGap ) || sinceLast >= kMaxRingGap ) {
            rings.push_back({ s, slope, DangerAt(holes, s) });
            sinceLast = 0.0f;
            accumAngle = 0.0f;
        }
    }
    rings.push_back({ len, slope, DangerAt(holes, len) });
    return rings;
}

// --- 1b. 境界リングの強制挿入 ---
//   穴の境界 d0/d1（キャップと隙間をピッタリ合わせる）、危険帯の境目（二重リングでvを切替）、
//   ジャンクションの切り詰め位置（パッチの入口断面と一致させる）にリングを必ず置く
void RoadSweepBuilder::InsertBoundaryRings(std::vector<Ring>& rings, float len, const std::vector<RoadCut>& cuts,
                                           const Holes& holes) const{
    struct Forced { float s; bool dual; };
    std::vector<Forced> forced;
    for ( const RoadCut& c : cuts ) {
        if ( c.s0 > 0.0f && c.s0 < len ) forced.push_back({ c.s0, false });
        if ( c.s1 > 0.0f && c.s1 < len ) forced.push_back({ c.s1, false });
    }
    for ( const auto& h : holes ) {
        float w0 = h.d0 - warnLength_, w1 = h.d1 + warnLength_;
        if ( w0 > 0.0f && w0 < len )     forced.push_back({ w0, true });  // 危険帯に入る（v切替）
        if ( h.d0 > 0.0f && h.d0 < len ) forced.push_back({ h.d0, false });
        if ( h.d1 > 0.0f && h.d1 < len ) forced.push_back({ h.d1, false });
        if ( w1 > 0.0f && w1 < len )     forced.push_back({ w1, true });  // 危険帯から出る
    }
    std::sort(forced.begin(), forced.end(), [](const Forced& a, const Forced& b){ return a.s < b.s; });

    for ( const Forced& fc : forced ) {
        // 挿入位置（s昇順を保つ）
        size_t at = 0;
        while ( at < rings.size() && rings[at].s < fc.s - 1e-4f ) { ++at; }
        bool slopeHere = ( at > 0 ) ? rings[at - 1].slope : rings.front().slope;
        if ( fc.dual ) {
            // 危険帯の境目：同位置の二重リング（手前側の状態 → 奥側の状態）
            rings.insert(rings.begin() + at, { fc.s, slopeHere, DangerAt(holes, fc.s + 1e-3f) });
            rings.insert(rings.begin() + at, { fc.s, slopeHere, DangerAt(holes, fc.s - 1e-3f) });
        } else {
            // 既に（ほぼ）同位置のリングがあれば挿入しない
            bool exists = ( at < rings.size() && std::abs(rings[at].s - fc.s) < 1e-3f ) ||
                          ( at > 0 && std::abs(rings[at - 1].s - fc.s) < 1e-3f );
            if ( !exists ) { rings.insert(rings.begin() + at, { fc.s, slopeHere, DangerAt(holes, fc.s) }); }
        }
    }
}

// --- 3. 区間の分類：描く / 穴 / ジャンクションの切り詰め / 二重リング ---
std::vector<RoadSweepBuilder::Seg> RoadSweepBuilder::ClassifySegments(const std::vector<Ring>& rings,
                                                                      const std::vector<RoadCut>& cuts,
                                                                      const Holes& holes){
    std::vector<Seg> segs(rings.size() - 1);
    for ( size_t i = 0; i + 1 < rings.size(); ++i ) {
        float mid = ( rings[i].s + rings[i + 1].s ) * 0.5f;
        if ( rings[i + 1].s - rings[i].s < 1e-4f ) { segs[i] = Seg::Degen; continue; }
        bool inCut = false;
        for ( const RoadCut& c : cuts ) {
            if ( mid >= c.s0 && mid <= c.s1 ) { inCut = true; break; }
        }
        bool inHole = false;
        for ( const auto& h : holes ) {
            if ( mid >= h.d0 && mid <= h.d1 ) { inHole = true; break; }
        }
        if ( inCut )       { segs[i] = Seg::Cut; }
        else if ( inHole ) { segs[i] = Seg::Hole; }
        else               { segs[i] = Seg::Draw; }
    }
    return segs;
}

// --- 5. 切り口に「奈落フタ」を張る（アトラスの黒帯＝暗色）---
//   以前は road_end の丸キャップで塞いでいたが、緑の丸い端が「地面が続いている」ように
//   見えて穴と認識できなかった。切り口を暗くすることで「落ちる」と直感できるようにする
void RoadSweepBuilder::EmitDarkCap(const SplineRail& rail, float s, const std::array<Vector3, 12>& ringPos,
                                   float facing, RoadMeshBuilder& out){
    SplineRail::RailFrame f = rail.GetFrameAtDistance(s);
    Vector3 nrm = Multiply(f.tangent, facing);
    uint32_t base = out.VertexCount();
    for ( int k = 0; k < 6; ++k ) {
        const ProfileV& pv = kProfile[kOutline[k]];
        // v固定＝黒帯（奈落色）
        out.AddVertex(ringPos[kOutline[k]], nrm, 0.10f + ( pv.lat + 1.0f ) * 0.15f, kDarkV);
    }
    for ( uint32_t t = 1; t <= 4; ++t ) {
        out.AddTriangle(base, base + t, base + t + 1);
    }
}

// レール1本ぶんの掃引メッシュを生成する
void RoadSweepBuilder::Build(const SplineRail& rail, const std::vector<RoadCut>& cuts, bool simple,
                             bool capFront, bool capBack, RoadMeshBuilder& out) const{
    const float len = rail.GetLength();
    const Holes holes = simple ? Holes {} : rail.GetHoleIntervals();

    // --- 1. リング位置の決定 ---
    std::vector<Ring> rings;
    if ( simple ) {
        rings = PlaceSimpleRings(len);
    } else {
        rings = PlaceAdaptiveRings(rail, len, holes);
        InsertBoundaryRings(rings, len, cuts, holes);
    }

    // --- 2. 頂点生成（フレーム×断面プロファイル。内側折返しは前リングへ溶接）---
    const uint32_t ringBase = out.VertexCount();
    out.ReserveVertices(ringBase + rings.size() * 12);
    std::vector<std::array<Vector3, 12>> ringPos(rings.size()); // 奈落フタ生成用に溶接後の位置を保存

    Vector3 prevPos[12] {};
    bool hasPrev = false;
    for ( size_t ri = 0; ri < rings.size(); ++ri ) {
        const Ring& ring = rings[ri];
        SplineRail::RailFrame f = rail.GetFrameAtDistance(ring.s);
        for ( int k = 0; k < 12; ++k ) {
            const ProfileV& pv = kProfile[k];
            Vector3 pos = Add(f.position,
                Add(Multiply(f.right, pv.lat), Multiply(f.up, pv.h + kTopOffset)));

            // 内側折返しの溶接：急カーブの内側で「前のリングより後ろ」に来たら前の位置に留める
            if ( !simple && hasPrev &&
                 Dot({ pos.x - prevPos[k].x, pos.y - prevPos[k].y, pos.z - prevPos[k].z },
                        f.tangent) < 0.0f ) {
                pos = prevPos[k];
            }
            prevPos[k] = pos;
            ringPos[ri][k] = pos;

            float v = pv.v;
            if ( k == 4 || k == 5 ) {
                if ( ring.danger )      { v = kDangerV[k - 4]; } // 危険帯（穴の手前後・上面のみ）
                else if ( ring.slope )  { v = kSlopeV[k - 4]; }  // 坂帯
            }

            out.AddVertex(pos, Normalize(Add(Multiply(f.right, pv.nr), Multiply(f.up, pv.nu))),
                          ring.s * kUvPerMeter, v);
        }
        hasPrev = true;
    }

    // --- 3. 区間の分類 ---
    const std::vector<Seg> segs = ClassifySegments(rings, cuts, holes);

    // --- 4. インデックス：Draw区間だけリング間に6帯×2三角形を張る ---
    for ( size_t i = 0; i + 1 < rings.size(); ++i ) {
        if ( segs[i] != Seg::Draw ) continue;
        uint32_t base0 = ringBase + static_cast<uint32_t>( i ) * 12;
        uint32_t base1 = ringBase + static_cast<uint32_t>( i + 1 ) * 12;
        for ( const auto& strip : kStrips ) {
            uint32_t a0 = base0 + strip[0], a1 = base0 + strip[1];
            uint32_t b0 = base1 + strip[0], b1 = base1 + strip[1];
            out.AddTriangle(a0, b0, b1);
            out.AddTriangle(a0, b1, a1);
        }
    }

    // --- 5. 穴の切り口／自由端の奈落フタ ---
    if ( simple ) return;
    // 穴区間の始まり/終わりのリング（Degenを透過して隣がDrawのところ）にフタ
    for ( size_t i = 0; i < segs.size(); ++i ) {
        if ( segs[i] != Seg::Hole ) continue;
        size_t prev = i;
        while ( prev > 0 && segs[prev - 1] == Seg::Degen ) { --prev; }
        if ( prev > 0 && segs[prev - 1] == Seg::Draw ) { EmitDarkCap(rail, rings[i].s, ringPos[i], +1.0f, out); }
        size_t next = i;
        while ( next + 1 < segs.size() && segs[next + 1] == Seg::Degen ) { ++next; }
        if ( next + 1 < segs.size() && segs[next + 1] == Seg::Draw ) {
            EmitDarkCap(rail, rings[i + 1].s, ringPos[i + 1], -1.0f, out);
        }
    }
    // レールの自由端（未接続の端）にも同じ平らなフタ。
    //   端の区間が穴/ジャンクション(Cut)なら面が無いのでフタも不要（自動スキップ）
    if ( capFront && !segs.empty() && segs.front() == Seg::Draw ) {
        EmitDarkCap(rail, rings.front().s, ringPos.front(), -1.0f, out); // 始端：外向き＝-tangent
    }
    if ( capBack && !segs.empty() && segs.back() == Seg::Draw ) {
        EmitDarkCap(rail, rings.back().s, ringPos.back(), +1.0f, out);   // 終端：外向き＝+tangent
    }
}

// ゆるい溶接の掃引コネクタ
void RoadSweepBuilder::BuildConnector(const std::array<Vector3, 5>& nodes, RoadMeshBuilder& out) const{
    // その場で小さなレール（Catmull-Rom）を合成して同じ断面で掃引する
    SplineRail connector;
    connector.nodes.assign(nodes.begin(), nodes.end());
    connector.BuildDistanceTable(); // FrameCache もここで自動構築される

    // 助走区間（pPre〜p0 と p1〜pPost）は本線の掃引と重なるので張らない
    float s0 = connector.GetDistanceFromT(1.0f);
    float s1 = connector.GetDistanceFromT(3.0f);
    std::vector<RoadCut> connCuts = { { 0.0f, s0 }, { s1, connector.GetLength() } };
    Build(connector, connCuts, false, false, false, out);
}
