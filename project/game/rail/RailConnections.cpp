#include "game/rail/RailConnections.h"
#include "engine/rail/SplineRail.h"

#include <algorithm>
#include <cmath>

namespace {
    const float kConnThreshold = 0.7f; // エディタの吸着半径と同じ（吸着済みノードは動かない）

    float EndDist(const Vector3& a, const Vector3& b){
        float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    Vector3 MidPoint(const Vector3& a, const Vector3& b){
        return { ( a.x + b.x ) * 0.5f, ( a.y + b.y ) * 0.5f, ( a.z + b.z ) * 0.5f };
    }

    // フェーズ0：ループ（円状レール）の検出。front と back が近ければ閉じたレール
    void DetectLoops(std::vector<SplineRail>& rails){
        for ( auto& rail : rails ) {
            rail.isLoop = false;
            if ( rail.nodes.size() < 3 ) continue;
            if ( EndDist(rail.nodes.front(), rail.nodes.back()) >= kConnThreshold ) continue;
            Vector3 mid = MidPoint(rail.nodes.front(), rail.nodes.back());
            rail.nodes.front() = mid;
            rail.nodes.back()  = mid;
            rail.isLoop = true;
            rail.BuildDistanceTable();
        }
    }

    // フェーズ1：最も近い端点同士で連結関係を決める（動くレールは除外）
    void FindEndConnections(std::vector<SplineRail>& rails){
        for ( int i = 0; i < ( int ) rails.size(); ++i ) {
            if ( rails[i].isLoop || rails[i].HasMotion() || rails[i].nodes.size() < 2 ) continue;
            const Vector3& iFront = rails[i].nodes.front();
            const Vector3& iBack  = rails[i].nodes.back();
            float bestFront = kConnThreshold;
            float bestBack  = kConnThreshold;
            for ( int j = 0; j < ( int ) rails.size(); ++j ) {
                if ( i == j || rails[j].HasMotion() || rails[j].nodes.size() < 2 ) continue;
                const Vector3& jFront = rails[j].nodes.front();
                const Vector3& jBack  = rails[j].nodes.back();

                float ff = EndDist(iFront, jFront);
                float fb = EndDist(iFront, jBack);
                if ( ff < bestFront ) { bestFront = ff; rails[i].frontConnIndex = j; rails[i].frontConnToFront = true; }
                if ( fb < bestFront ) { bestFront = fb; rails[i].frontConnIndex = j; rails[i].frontConnToFront = false; }

                float bf = EndDist(iBack, jFront);
                float bb = EndDist(iBack, jBack);
                if ( bf < bestBack ) { bestBack = bf; rails[i].backConnIndex = j; rails[i].backConnToFront = true; }
                if ( bb < bestBack ) { bestBack = bb; rails[i].backConnIndex = j; rails[i].backConnToFront = false; }
            }
        }
    }

    // フェーズ2：連結端点を溶接（隙間を物理的に詰める）。ノードを動かしたら true。
    //   【相互合意チェック】i→j と j→i が互いに選び合っている組だけ「中点」へ溶接する。
    //   片思い（jは別のレールkの方が近い）の場合、双方を別々の中点へ動かすと
    //   数cmの隙間/段差が残るバグがあった。片思い側は相手の"今の"端点位置へ
    //   ぴったりスナップさせることで、必ず一致させる。
    bool WeldEnds(std::vector<SplineRail>& rails){
        struct Ends { Vector3 front, back; bool valid; };
        std::vector<Ends> orig(rails.size());
        for ( int i = 0; i < ( int ) rails.size(); ++i ) {
            if ( rails[i].nodes.size() < 2 ) { orig[i].valid = false; continue; }
            orig[i] = { rails[i].nodes.front(), rails[i].nodes.back(), true };
        }
        // レール j の端(front/back)が「レール i の端」を選び返しているか
        auto mutual = [&](int i, bool iIsFront, int j, bool jEndIsFront) -> bool{
            int  jConn      = jEndIsFront ? rails[j].frontConnIndex   : rails[j].backConnIndex;
            bool jConnFront = jEndIsFront ? rails[j].frontConnToFront : rails[j].backConnToFront;
            return jConn == i && jConnFront == iIsFront;
        };

        bool moved = false;
        // パスA：相互ペアは中点へ（両者同じ点になる）
        // パスB：片思いの端は、相手の「今の（溶接後の）」端点位置へぴったりスナップ
        for ( int pass = 0; pass < 2; ++pass ) {
            const bool wantMutual = ( pass == 0 );
            for ( int i = 0; i < ( int ) rails.size(); ++i ) {
                if ( !orig[i].valid ) continue;
                for ( int end = 0; end < 2; ++end ) {
                    const bool isFront = ( end == 0 );
                    const int  j       = isFront ? rails[i].frontConnIndex   : rails[i].backConnIndex;
                    const bool toFront = isFront ? rails[i].frontConnToFront : rails[i].backConnToFront;
                    if ( j < 0 || !orig[j].valid ) continue;
                    if ( mutual(i, isFront, j, toFront) != wantMutual ) continue;
                    Vector3& node = isFront ? rails[i].nodes.front() : rails[i].nodes.back();
                    if ( wantMutual ) {
                        node = MidPoint(isFront ? orig[i].front : orig[i].back, toFront ? orig[j].front : orig[j].back);
                    } else {
                        node = toFront ? rails[j].nodes.front() : rails[j].nodes.back();
                    }
                    moved = true;
                }
            }
        }
        return moved;
    }

    // フェーズ4：途中分岐（branchPoints）の構築。
    //   レール j の端点が、レール i の「途中（端から離れた本体）」に接している T字路 を検出し、
    //   レール i 側に分岐点として登録する。同タイプ同士のT字路でも乗り換えできるようになる。
    void FindBranchPoints(std::vector<SplineRail>& rails){
        for ( int j = 0; j < ( int ) rails.size(); ++j ) {
            const SplineRail& rj = rails[j];
            if ( rj.nodes.size() < 2 || rj.isLoop || rj.HasMotion() ) continue;

            struct EndInfo { Vector3 pos; bool isFront; int connIdx; };
            const EndInfo ends[2] = {
                { rj.nodes.front(), true,  rj.frontConnIndex },
                { rj.nodes.back(),  false, rj.backConnIndex  },
            };
            for ( const auto& e : ends ) {
                if ( e.connIdx >= 0 ) continue; // 端点同士で連結済みなら分岐扱いにしない
                for ( int i = 0; i < ( int ) rails.size(); ++i ) {
                    if ( i == j ) continue;
                    SplineRail& ri = rails[i];
                    if ( ri.nodes.size() < 2 || ri.HasMotion() ) continue;
                    float len = ri.GetLength();
                    if ( len < 1.5f ) continue;

                    float cd = ri.GetClosestDistance(e.pos);
                    if ( cd < 0.6f || cd > len - 0.6f ) continue; // 端付近は端点連結の領分
                    Vector3 cp = ri.GetPositionByDistance(cd);
                    if ( EndDist(cp, e.pos) > kConnThreshold ) continue;

                    // 分岐キーの向き：接合点から j 側へ少し入った点の「交差軸」変位で決める
                    //   i が横レール → W/S(±Z) で分岐 / i が縦レール → D/A(±X) で分岐
                    float probe = ( std::min )( 1.0f, rj.GetLength() * 0.5f );
                    Vector3 into = rj.GetPositionByDistance(e.isFront ? probe : rj.GetLength() - probe);
                    float cross = ( ri.type == SplineRail::RailType::Horizontal ) ? ( into.z - cp.z )
                                                                                  : ( into.x - cp.x );
                    if ( std::abs(cross) < 0.3f ) continue; // 交差方向が曖昧（ほぼ平行）→キーに割当不能

                    SplineRail::BranchPoint bp;
                    bp.distance   = cd;
                    bp.targetRail = j;
                    bp.targetDist = e.isFront ? 0.0f : rj.GetLength();
                    bp.zSign      = ( cross >= 0.0f ) ? +1 : -1;
                    ri.branchPoints.push_back(bp);
                    break; // この端は登録済み。他のレールへの多重登録はしない
                }
            }
        }
    }
}

void RailConnections::Build(std::vector<SplineRail>& rails){
    for ( auto& rail : rails ) {
        rail.frontConnIndex = -1;
        rail.backConnIndex  = -1;
        rail.branchPoints.clear();
    }
    DetectLoops(rails);
    FindEndConnections(rails);
    // フェーズ3：ノードを動かしたので距離テーブルを作り直す
    if ( WeldEnds(rails) ) {
        for ( auto& rail : rails ) { rail.BuildDistanceTable(); }
    }
    FindBranchPoints(rails);
}
