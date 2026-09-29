#include "game/rail/road/RoadJunctionPatchBuilder.h"

#include "game/rail/road/RoadMeshBuilder.h"
#include "game/rail/road/RoadProfile.h"
#include "engine/math/VectorMath.h"

#include <algorithm>
#include <cmath>

using namespace VectorMath;
using namespace RoadProfile;
using namespace RoadJunctionGeom;

namespace {

// --- パッチ固有のパラメータ（GUIDE_ジャンクション生成）---
const float kArcMinDeg  = 195.0f; // これ超は外周円弧
const float kArcStepDeg = 12.0f;  // 円弧の刻み

// 上面の平面マッピング先（アトラスの無地帯）
const float kPlainTopV0    = 0.89f;
const float kPlainTopVSpan = 0.09f;

// 点pの「外向き水平方向」（ノード中心 N から離れる向き）
Vector3 OutwardFromNode(const Vector3& p, const Vector3& N){
    Vector3 o = { p.x - N.x, 0.0f, p.z - N.z };
    float l = Length(o);
    if ( l < 1e-4f ) return { 1.0f, 0.0f, 0.0f };
    return Multiply(o, 1.0f / l);
}

} // namespace

// 各ウェッジのモードと円弧分割数を先に決める（内外の点列数を一致させるため）
RoadJunctionPatchBuilder::Wedge RoadJunctionPatchBuilder::DecideWedge(const Vector3& di, const Vector3& dj) const{
    const float phiDeg = WedgeAngleDeg(di, dj);
    Wedge w;
    w.mode = ( phiDeg < kMiterMaxDeg ) ? WedgeMode::Miter
                                       : ( phiDeg > kArcMinDeg ? WedgeMode::Arc : WedgeMode::Straight );
    // エディタの「曲がり角の形」設定で上書き：1=いつも丸広場（ヨッシー風）/ 2=丸なし
    //   （165°〜195°の「ほぼ直進」だけは、丸を強制すると逆に不自然なので自動のまま）
    if ( cornerStyle_ == 1 && w.mode == WedgeMode::Miter ) { w.mode = WedgeMode::Arc; }
    if ( cornerStyle_ == 2 && w.mode == WedgeMode::Arc )   { w.mode = WedgeMode::Miter; }
    w.arcSteps = ( std::max )( 2, static_cast<int>( phiDeg / kArcStepDeg ) );
    return w;
}

// 腕 i の入口断面上の点（lat = 左方向を+とした符号付き幅、h = 断面高さ）
//   入口はレールの実フレームから取る → 掃引側の入口リングと完全一致（継ぎ目ゼロ）
Vector3 RoadJunctionPatchBuilder::EntryPoint(const Patch& p, int i, float lat, float h){
    const Patch::ArmGeo& g = p.geo[i];
    return Add(g.frame.position,
                  Add(Multiply(g.frame.right, g.leftSign * lat),
                         Multiply(g.frame.up, h + kTopOffset)));
}

// ウェッジ (i → j=i+1) の境界点列。幅 w・高さ h。マイター / 直進 / 円弧
std::vector<Vector3> RoadJunctionPatchBuilder::WedgePath(const Patch& p, int i, float w, float h){
    const RoadJunction& junc = *p.junc;
    const Vector3& N = junc.center;
    const int j = ( i + 1 ) % static_cast<int>( junc.arms.size() );
    const Wedge& wedge = p.wedges[i];

    std::vector<Vector3> path;
    Vector3 a = EntryPoint(p, i, +w, h);        // 道 i の（ウェッジ側=左）縁
    Vector3 b = EntryPoint(p, j, -w, h);        // 道 j の（ウェッジ側=右）縁
    float y = N.y + h + kTopOffset;             // 内部点はノード高さ基準（付近は平坦に保つ運用）
    path.push_back(a);
    if ( wedge.mode == WedgeMode::Miter ) {
        float t = 0.0f, s = 0.0f;
        if ( SolveMiter(junc.arms[i].dir, junc.arms[j].dir, w, t, s) && t > 0.0f && s > 0.0f ) {
            Vector3 C = Add(N, Add(Multiply(junc.arms[i].dir, t),
                                         Multiply(LeftOf(junc.arms[i].dir), w)));
            path.push_back({ C.x, y, C.z });
        }
    } else if ( wedge.mode == WedgeMode::Arc ) {
        // aからbへNを中心に回る円弧（半径は端点間で線形補間）
        float angA = std::atan2(a.z - N.z, a.x - N.x);
        float angB = std::atan2(b.z - N.z, b.x - N.x);
        float delta = angB - angA;
        while ( delta <= 0.0f ) { delta += 2.0f * kPi; }
        float rA = std::sqrt(( a.x - N.x ) * ( a.x - N.x ) + ( a.z - N.z ) * ( a.z - N.z ));
        float rB = std::sqrt(( b.x - N.x ) * ( b.x - N.x ) + ( b.z - N.z ) * ( b.z - N.z ));
        for ( int k = 1; k < wedge.arcSteps; ++k ) {
            float u = static_cast<float>( k ) / wedge.arcSteps;
            float ang = angA + delta * u;
            float r = rA + ( rB - rA ) * u;
            path.push_back({ N.x + std::cos(ang) * r, y, N.z + std::sin(ang) * r });
        }
    }
    path.push_back(b);
    return path;
}

// 全ウェッジの境界をつないだ閉ループ
std::vector<Vector3> RoadJunctionPatchBuilder::Loop(const Patch& p, float w, float h){
    std::vector<Vector3> loop;
    const int n = static_cast<int>( p.junc->arms.size() );
    for ( int i = 0; i < n; ++i ) {
        std::vector<Vector3> path = WedgePath(p, i, w, h);
        loop.insert(loop.end(), path.begin(), path.end());
    }
    return loop;
}

// 上面/底面：境界ループをノード中心から扇状に張る。
//   平面マッピング（アトラスの無地帯へ。REPEATで破綻しないよう帯内でwrap）
void RoadJunctionPatchBuilder::EmitFan(const Patch& p, float w, float h, const Vector3& normal,
                                       float v0, float vSpan, bool flip, RoadMeshBuilder& out){
    const Vector3& N = p.junc->center;
    auto planarU = [&](const Vector3& q){ return ( q.x - N.x ) * kUvPerMeter; };
    auto planarV = [&](const Vector3& q){ return v0 + Fract(( q.z - N.z ) * kUvPerMeter) * vSpan; };

    std::vector<Vector3> loop = Loop(p, w, h);
    Vector3 centerP = { N.x, N.y + h + kTopOffset, N.z };
    uint32_t c = out.AddVertex(centerP, normal, planarU(centerP), planarV(centerP));
    std::vector<uint32_t> ids(loop.size());
    for ( size_t m = 0; m < loop.size(); ++m ) {
        ids[m] = out.AddVertex(loop[m], normal, planarU(loop[m]), planarV(loop[m]));
    }
    for ( size_t m = 0; m < loop.size(); ++m ) {
        uint32_t cur = ids[m], nxt = ids[( m + 1 ) % loop.size()];
        if ( flip ) { out.AddTriangle(c, nxt, cur); }
        else        { out.AddTriangle(c, cur, nxt); }
    }
}

// ベベル＋壁（ウェッジ部分のみ。入口には張らない＝道の断面がそのまま入る）
void RoadJunctionPatchBuilder::EmitBevelAndWalls(const Patch& p, RoadMeshBuilder& out){
    const Vector3& N = p.junc->center;
    const int n = static_cast<int>( p.junc->arms.size() );
    for ( int i = 0; i < n; ++i ) {
        std::vector<Vector3> pin  = WedgePath(p, i, kInnerWidth, kTopH);
        std::vector<Vector3> pout = WedgePath(p, i, kHalfWidth,  kBevelH);
        std::vector<Vector3> pbot = WedgePath(p, i, kHalfWidth,  0.0f);
        size_t cnt = ( std::min )( { pin.size(), pout.size(), pbot.size() } );
        if ( cnt < 2 ) continue;

        // 累積距離を u に使う
        float accum = 0.0f;
        std::vector<float> us(cnt, 0.0f);
        for ( size_t m = 1; m < cnt; ++m ) {
            accum += Length(Subtract(pout[m], pout[m - 1]));
            us[m] = accum * kUvPerMeter;
        }

        for ( size_t m = 0; m + 1 < cnt; ++m ) {
            // ベベル（上0.7422 / 下0.7070。法線= 外向き0.64 + 上0.77）
            Vector3 n0 = Normalize(Add(Multiply(OutwardFromNode(pout[m], N), kBevelNr), Vector3 { 0.0f, kBevelNu, 0.0f }));
            Vector3 n1 = Normalize(Add(Multiply(OutwardFromNode(pout[m + 1], N), kBevelNr), Vector3 { 0.0f, kBevelNu, 0.0f }));
            uint32_t i0 = out.AddVertex(pin[m],      n0, us[m],     kBevelTopV);
            uint32_t i1 = out.AddVertex(pin[m + 1],  n1, us[m + 1], kBevelTopV);
            uint32_t o0 = out.AddVertex(pout[m],     n0, us[m],     kBevelBottomV);
            uint32_t o1 = out.AddVertex(pout[m + 1], n1, us[m + 1], kBevelBottomV);
            out.AddTriangle(i0, o0, o1); out.AddTriangle(i0, o1, i1);

            // 壁（上0.6992 / 下0.5352。法線=外向き水平）
            Vector3 w0 = OutwardFromNode(pout[m], N);
            Vector3 w1 = OutwardFromNode(pout[m + 1], N);
            uint32_t t0 = out.AddVertex(pout[m],     w0, us[m],     kWallTopV);
            uint32_t t1 = out.AddVertex(pout[m + 1], w1, us[m + 1], kWallTopV);
            uint32_t b0 = out.AddVertex(pbot[m],     w0, us[m],     kWallBottomV);
            uint32_t b1 = out.AddVertex(pbot[m + 1], w1, us[m + 1], kWallBottomV);
            out.AddTriangle(t0, b0, b1); out.AddTriangle(t0, b1, t1);
        }
    }
}

// 1ジャンクションのパッチ（上面扇+ベベル+壁+底）を生成する（GUIDE_ジャンクション生成 §4）
void RoadJunctionPatchBuilder::Build(const std::vector<SplineRail>& rails, const RoadJunction& junc,
                                     RoadMeshBuilder& out) const{
    const int n = static_cast<int>( junc.arms.size() );
    if ( n < 1 ) return; // n==1 は「端の丸広場」（1本腕＝入口1つ＋360°の円弧ウェッジ）

    Patch p;
    p.junc = &junc;
    // 各腕の入口フレームと「左側の符号」（フレームrightが世界の左とどちら向きで一致するか）
    p.geo.resize(n);
    for ( int i = 0; i < n; ++i ) {
        const RoadArm& arm = junc.arms[i];
        p.geo[i].frame = rails[arm.rail].GetFrameAtDistance(arm.cutS);
        p.geo[i].leftSign = ( Dot(p.geo[i].frame.right, LeftOf(arm.dir)) >= 0.0f ) ? 1.0f : -1.0f;
    }
    p.wedges.resize(n);
    for ( int i = 0; i < n; ++i ) {
        p.wedges[i] = DecideWedge(junc.arms[i].dir, junc.arms[( i + 1 ) % n].dir);
    }

    // --- 上面：内側境界(W-B)のループをノード中心から扇状に張る ---
    EmitFan(p, kInnerWidth, kTopH, { 0.0f, 1.0f, 0.0f }, kPlainTopV0, kPlainTopVSpan, false, out);
    // --- ベベル＋壁 ---
    EmitBevelAndWalls(p, out);
    // --- 底面：外周(W)のループをノード中心（高さ0）から扇状に張る（下向き＝巻き順反転）---
    EmitFan(p, kHalfWidth, 0.0f, { 0.0f, -1.0f, 0.0f }, kBottomV0, kBottomV1 - kBottomV0, true, out);
}
