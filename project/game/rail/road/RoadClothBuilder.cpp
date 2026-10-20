#include "game/rail/road/RoadClothBuilder.h"

#include "game/rail/road/RoadMeshBuilder.h"
#include "engine/math/VectorMath.h"
#include "engine/rail/SplineRail.h"

#include <algorithm>
#include <cmath>

using namespace VectorMath;

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kStep = 0.15f;        // リングの間隔(m)
    constexpr float kSkirtOut = 0.012f;   // すそが端より外へ出る量(m)
    constexpr float kGroundDepth = 0.075f; // 布のみ：すそを地面の中へ入れる深さ（背景の地面はレール面の 6cm 下）
    constexpr int   kCapSegments = 14;    // 丸い端の分割数（半円）

    // 断面の1点（レールの横方向 lat・レール面からの高さ h・テクスチャの v）
    struct Section { float lat, h, v; };

    // レール上の1リング（位置・向き・柄の u）
    struct Ring { Vector3 pos, right, up, tangent; float u; };

    Ring RingAt(const SplineRail& rail, float s){
        const SplineRail::RailFrame f = rail.GetFrameAtDistance(s);
        return { f.position, f.right, f.up, f.tangent, s * RoadClothBuilder::kUvPerMeter };
    }
    Vector3 At(const Ring& ring, float lat, float h){
        return Add(ring.pos, Add(Multiply(ring.right, lat), Multiply(ring.up, h)));
    }
}

RoadClothBuilder::EndInfo RoadClothBuilder::EndOf(const std::vector<SplineRail>& rails, int railIndex, bool front){
    const SplineRail& rail = rails[railIndex];
    EndInfo info { false, ( rail.endPlaza & ( front ? 1 : 2 ) ) != 0 };
    if ( rail.isLoop ) { info.connected = true; return info; }
    if ( ( front ? rail.frontConnIndex : rail.backConnIndex ) >= 0 ) { info.connected = true; return info; }
    // T字：端がほかのレールの本体に載っている（RailConnections が載せてある）
    const Vector3 end = rail.GetFrameAtDistance(front ? 0.0f : rail.GetLength()).position;
    for ( int j = 0; j < ( int ) rails.size(); ++j ) {
        if ( j == railIndex || rails[j].nodes.size() < 2 || rails[j].GetLength() <= 0.0f ) continue;
        const float s = rails[j].GetClosestDistance(end);
        const Vector3 p = rails[j].GetFrameAtDistance(s).position;
        if ( Length({ p.x - end.x, p.y - end.y, p.z - end.z }) < 0.05f ) { info.connected = true; break; }
    }
    return info;
}

void RoadClothBuilder::Build(const std::vector<SplineRail>& rails, int railIndex, const std::vector<Range>& visible,
                             float extraLift, RoadMeshBuilder& out) const{
    const SplineRail& rail = rails[railIndex];
    const float length = rail.GetLength();
    if ( length <= 0.0f || rail.nodes.size() < 2 ) return;
    const RailSkin& skin = rail.skin;
    const float hw = ( std::max )( skin.width * 0.5f, 0.2f );
    const float hem = ( std::min )( 0.12f, hw * 0.3f );
    const float lift = skin.lift + extraLift;
    const float depth = ( skin.style == RailSkin::ClothOnBase ) ? 0.0f : kGroundDepth;
    const Section section[6] = {
        { -hw - kSkirtOut, -depth,             0.0f },
        { -hw,             lift + skin.curl,   0.035f },
        { -hw + hem,       lift,               0.10f },
        {  hw - hem,       lift,               0.90f },
        {  hw,             lift + skin.curl,   0.965f },
        {  hw + kSkirtOut, -depth,             1.0f },
    };

    // 敷く範囲から穴の区間を抜く
    std::vector<Range> spans;
    const auto holes = rail.GetHoleIntervals();
    for ( Range r : visible ) {
        r.s0 = std::clamp(r.s0, 0.0f, length);
        r.s1 = std::clamp(r.s1, 0.0f, length);
        std::vector<Range> pieces { r };
        for ( const auto& hole : holes ) {
            std::vector<Range> next;
            for ( const Range& p : pieces ) {
                if ( hole.d1 <= p.s0 || hole.d0 >= p.s1 ) { next.push_back(p); continue; }
                if ( hole.d0 > p.s0 ) next.push_back({ p.s0, hole.d0 });
                if ( hole.d1 < p.s1 ) next.push_back({ hole.d1, p.s1 });
            }
            pieces = next;
        }
        for ( const Range& p : pieces ) { if ( p.s1 - p.s0 > 0.02f ) spans.push_back(p); }
    }
    if ( spans.empty() ) return;

    const EndInfo frontEnd = EndOf(rails, railIndex, true);
    const EndInfo backEnd = EndOf(rails, railIndex, false);
    const float fullEps = 1e-3f;

    auto vertexNormal = [&](const Ring& ring, int k) -> Vector3 {
        if ( k == 2 || k == 3 ) return ring.up;
        const float side = ( k < 3 ) ? -1.0f : 1.0f;
        return Normalize(Add(ring.up, Multiply(ring.right, side * ( ( k == 0 || k == 5 ) ? 2.0f : 0.6f ))));
    };
    auto emitRing = [&](const Ring& ring){
        for ( int k = 0; k < 6; ++k ) {
            out.AddVertex(At(ring, section[k].lat, section[k].h), vertexNormal(ring, k), ring.u, section[k].v);
        }
    };

    // 丸い端・丸広場：中心から外へ（内側のステッチ・めくれ・すそ）。arcFrom〜arcTo はラジアン（out 向きが 0）
    auto emitDisc = [&](const Ring& ring, const Vector3& outDir, float radius, float hemWidth, float arcFrom, float arcTo,
                        float discLift){
        const int segments = ( int ) std::ceil(kCapSegments * ( arcTo - arcFrom ) / kPi);
        const Section polar[4] = { { 0.0f, discLift, 0.5f }, { radius - hemWidth, discLift, 0.10f },
                                   { radius, discLift + skin.curl, 0.035f }, { radius + kSkirtOut, -depth, 0.0f } };
        uint32_t prev[4] {};
        for ( int i = 0; i <= segments; ++i ) {
            const float theta = arcFrom + ( arcTo - arcFrom ) * ( float ) i / segments;
            const Vector3 dir = Add(Multiply(outDir, std::cos(theta)), Multiply(ring.right, std::sin(theta)));
            const float u = ring.u + theta * radius * kUvPerMeter;
            uint32_t now[4];
            for ( int k = 0; k < 4; ++k ) {
                const Vector3 pos = Add(Add(ring.pos, Multiply(dir, polar[k].lat)), Multiply(ring.up, polar[k].h));
                const Vector3 normal = ( k < 2 ) ? ring.up : Normalize(Add(ring.up, Multiply(dir, k == 3 ? 2.0f : 0.6f)));
                now[k] = out.AddVertex(pos, normal, u, polar[k].v);
            }
            if ( i > 0 ) {
                out.AddTriangle(prev[0], prev[1], now[1]);
                for ( int k = 1; k < 3; ++k ) {
                    out.AddTriangle(prev[k], prev[k + 1], now[k + 1]);
                    out.AddTriangle(prev[k], now[k + 1], now[k]);
                }
            }
            for ( int k = 0; k < 4; ++k ) prev[k] = now[k];
        }
    };

    for ( const Range& span : spans ) {
        const bool atFront = span.s0 <= fullEps;
        const bool atBack = span.s1 >= length - fullEps;
        // つながっている端は、端の向きへ半幅ぶん伸ばして相手の布と重ねる
        const float extendFront = ( atFront && frontEnd.connected ) ? hw : 0.0f;
        const float extendBack = ( atBack && backEnd.connected ) ? hw : 0.0f;

        std::vector<Ring> rings;
        if ( extendFront > 0.0f ) {
            Ring r = RingAt(rail, span.s0);
            r.pos = Add(r.pos, Multiply(r.tangent, -extendFront));
            r.u -= extendFront * kUvPerMeter;
            rings.push_back(r);
        }
        const float spanLength = span.s1 - span.s0;
        const int count = ( std::max )( 1, ( int ) std::ceil(spanLength / ( std::max )( kStep, length / 600.0f )) );
        for ( int i = 0; i <= count; ++i ) {
            rings.push_back(RingAt(rail, span.s0 + spanLength * ( float ) i / count));
        }
        if ( extendBack > 0.0f ) {
            Ring r = RingAt(rail, span.s1);
            r.pos = Add(r.pos, Multiply(r.tangent, extendBack));
            r.u += extendBack * kUvPerMeter;
            rings.push_back(r);
        }

        const uint32_t base = out.VertexCount();
        for ( const Ring& ring : rings ) { emitRing(ring); }
        for ( size_t i = 0; i + 1 < rings.size(); ++i ) {
            const uint32_t a = base + ( uint32_t ) i * 6, b = a + 6;
            for ( int k = 0; k < 5; ++k ) {
                out.AddTriangle(a + k, b + k, b + k + 1);
                out.AddTriangle(a + k, b + k + 1, a + k + 1);
            }
        }

        // 端の形
        auto finishEnd = [&](bool isFront){
            const bool atRailEnd = isFront ? atFront : atBack;
            const EndInfo& info = isFront ? frontEnd : backEnd;
            const Ring& ring = isFront ? rings[extendFront > 0.0f ? 1 : 0] : rings[rings.size() - ( extendBack > 0.0f ? 2 : 1 )];
            const Vector3 outDir = Multiply(ring.tangent, isFront ? -1.0f : 1.0f);
            if ( atRailEnd && info.plaza ) {
                const float radius = ( std::max )( hw * 1.6f, 1.3f );
                emitDisc(ring, outDir, radius, hem, -kPi, kPi, lift + 0.002f);
                return;
            }
            if ( atRailEnd && info.connected ) return; // 伸ばして相手と重ねた
            if ( atRailEnd ) {
                emitDisc(ring, outDir, hw, hem, -kPi * 0.5f, kPi * 0.5f, lift); // 丸い端
                return;
            }
            // 穴・展開の途中で切れる所：平らな切り口
            const uint32_t first = base + ( isFront ? 0u : ( uint32_t ) ( rings.size() - 1 ) * 6 );
            const uint32_t cap = out.VertexCount();
            for ( int k = 0; k < 6; ++k ) {
                const Ring& edgeRing = isFront ? rings.front() : rings.back();
                out.AddVertex(At(edgeRing, section[k].lat, section[k].h), outDir, edgeRing.u, section[k].v);
            }
            ( void ) first;
            out.AddTriangle(cap + 0, cap + 1, cap + 2);
            out.AddTriangle(cap + 0, cap + 2, cap + 3);
            out.AddTriangle(cap + 0, cap + 3, cap + 5);
            out.AddTriangle(cap + 3, cap + 4, cap + 5);
        };
        finishEnd(true);
        finishEnd(false);
    }
}
