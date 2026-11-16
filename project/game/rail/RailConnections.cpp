#include "game/rail/RailConnections.h"
#include "engine/math/VectorMath.h"
#include "engine/rail/SplineRail.h"

#include <algorithm>
#include <cmath>

namespace {
    const float kConnThreshold = 0.7f; // エディタの吸着半径と同じ（吸着済みノードは動かない）

    Vector3 MidPoint(const Vector3& a, const Vector3& b){
        return { ( a.x + b.x ) * 0.5f, ( a.y + b.y ) * 0.5f, ( a.z + b.z ) * 0.5f };
    }

    // フェーズ0：ループ（円状レール）の検出。front と back が近ければ閉じたレール
    void DetectLoops(std::vector<SplineRail>& rails){
        for ( auto& rail : rails ) {
            rail.isLoop = false;
            if ( rail.nodes.size() < 3 ) continue;
            if ( VectorMath::Distance(rail.nodes.front(), rail.nodes.back()) >= kConnThreshold ) continue;
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

                float ff = VectorMath::Distance(iFront, jFront);
                float fb = VectorMath::Distance(iFront, jBack);
                if ( ff < bestFront ) { bestFront = ff; rails[i].frontConnIndex = j; rails[i].frontConnToFront = true; }
                if ( fb < bestFront ) { bestFront = fb; rails[i].frontConnIndex = j; rails[i].frontConnToFront = false; }

                float bf = VectorMath::Distance(iBack, jFront);
                float bb = VectorMath::Distance(iBack, jBack);
                if ( bf < bestBack ) { bestBack = bf; rails[i].backConnIndex = j; rails[i].backConnToFront = true; }
                if ( bb < bestBack ) { bestBack = bb; rails[i].backConnIndex = j; rails[i].backConnToFront = false; }
            }
        }
    }

    // フェーズ1.5：つなぎ目の切れ端を残さない。ノードを動かしたら true。
    //   レール i の端が、相手 j の端よりも j の「2番目の点」の方に近い時（エディタで i の端を j の2番目の点へ吸着させた等）、
    //   そのまま端どうしを溶接すると、j の最初の短い区間が i の端の向こうへ切れ端として残り、
    //   道が折り返して重なったり、急な曲がりでねじれたりする。
    //   j の端を2番目の点のすぐ手前（同じ向きの延長線上）へ寄せて、切れ端を無くしてから溶接する。
    //   実行時のレールだけを直す（エディタのデータは変えない。点の数も変えないので、点の番号で置いた物はずれない）
    bool TrimStubEnds(std::vector<SplineRail>& rails){
        constexpr float kKeep = 0.05f; // 2番目の点の手前に残す長さ(m)
        bool moved = false;
        for ( int i = 0; i < ( int ) rails.size(); ++i ) {
            if ( rails[i].nodes.size() < 2 ) continue;
            for ( int end = 0; end < 2; ++end ) {
                const bool isFront = ( end == 0 );
                const int  j       = isFront ? rails[i].frontConnIndex   : rails[i].backConnIndex;
                const bool toFront = isFront ? rails[i].frontConnToFront : rails[i].backConnToFront;
                if ( j < 0 || j == i ) continue;
                auto& nodes = rails[j].nodes;
                const size_t n = nodes.size();
                if ( n < 3 ) continue;
                const Vector3& tip = isFront ? rails[i].nodes.front() : rails[i].nodes.back();
                Vector3& jEnd = toFront ? nodes.front() : nodes.back();
                const Vector3& second = toFront ? nodes[1] : nodes[n - 2];
                const Vector3& third = toFront ? nodes[2] : nodes[n - 3];
                if ( VectorMath::Distance(tip, second) + 1e-4f >= VectorMath::Distance(tip, jEnd) ) continue;
                const float len = VectorMath::Distance(second, third);
                if ( len < 1e-4f ) continue;
                const Vector3 dir { ( second.x - third.x ) / len, ( second.y - third.y ) / len, ( second.z - third.z ) / len };
                jEnd = { second.x + dir.x * kKeep, second.y + dir.y * kKeep, second.z + dir.z * kKeep };
                moved = true;
            }
        }
        return moved;
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

    // フェーズ2.5：端どうしのつなぎ目で、向きをなめらかにつなぐ（実行時のレールだけ。エディタのデータは変えない）。
    //   互いに選び合って溶接した 2 本の端に、共通の向きを渡す。2 本が 1 本の曲線のようにつながり、つなぎ目で向きが同じになる。
    //   共通の向き＝両側の「つなぎ目の隣の区間」の向きを混ぜたもの。短い区間の向きを重く見る
    //   （曲がりを長い区間の側へ回す。短い区間に曲がりを押し込むと、そこだけ急に曲がる）
    //   （前はそれぞれ自分の最後の区間の向きのままで、坂 45° → ゆるい坂 12° のつなぎ目では、
    //     歩いていると 1 フレームで落ちる速さが -3.6 → -1.1m/s に変わり、道も折れて見えた）。
    //   わざと曲げた角（折れ線の向きが 45° 以上違う）と、直線でつなぐレールは、そのまま角にする。入れたら true
    bool SmoothWeldedEnds(std::vector<SplineRail>& rails){
        constexpr float kCornerCos = 0.7071f; // 45°。これ以上曲がっていたら角（乗り換えデバッガの「角」と同じ）
        for ( auto& rail : rails ) { rail.hasFrontTangent = false; rail.hasBackTangent = false; }
        bool changed = false;
        for ( int i = 0; i < ( int ) rails.size(); ++i ) {
            SplineRail& a = rails[i];
            if ( a.nodes.size() < 2 || a.isLoop || a.HasMotion() || a.lineMode != 0 ) continue;
            for ( int end = 0; end < 2; ++end ) {
                const bool aFront = ( end == 0 );
                const int  j      = aFront ? a.frontConnIndex : a.backConnIndex;
                if ( j <= i ) continue; // つながっていない・自分自身・相手の側で処理済み
                SplineRail& b = rails[j];
                if ( b.nodes.size() < 2 || b.isLoop || b.HasMotion() || b.lineMode != 0 ) continue;
                const bool bFront = aFront ? a.frontConnToFront : a.backConnToFront;
                // 互いに選び合っている組だけ（片思いの端は、相手の曲線を変えない）
                const int  backIndex = bFront ? b.frontConnIndex : b.backConnIndex;
                const bool backFront = bFront ? b.frontConnToFront : b.backConnToFront;
                if ( backIndex != i || backFront != aFront ) continue;

                const Vector3& joint = aFront ? a.nodes.front() : a.nodes.back();
                const Vector3& aNext = aFront ? a.nodes[1] : a.nodes[a.nodes.size() - 2]; // つなぎ目の次の点（自分の側）
                const Vector3& bNext = bFront ? b.nodes[1] : b.nodes[b.nodes.size() - 2]; // つなぎ目の次の点（相手の側）
                const float lenA = VectorMath::Distance(joint, aNext), lenB = VectorMath::Distance(joint, bNext);
                if ( lenA < 1e-3f || lenB < 1e-3f ) continue;
                // a からつなぎ目へ入る向きと、つなぎ目から b へ出る向き
                const float cosTurn = ( ( joint.x - aNext.x ) * ( bNext.x - joint.x ) + ( joint.y - aNext.y ) * ( bNext.y - joint.y )
                                      + ( joint.z - aNext.z ) * ( bNext.z - joint.z ) ) / ( lenA * lenB );
                if ( cosTurn < kCornerCos ) continue; // 角はそのまま

                // a から b へ進む向き（長さ1）：両側の区間の向きを、相手の区間の長さで重み付けして混ぜる
                Vector3 through {
                    ( joint.x - aNext.x ) / lenA * lenB + ( bNext.x - joint.x ) / lenB * lenA,
                    ( joint.y - aNext.y ) / lenA * lenB + ( bNext.y - joint.y ) / lenB * lenA,
                    ( joint.z - aNext.z ) / lenA * lenB + ( bNext.z - joint.z ) / lenB * lenA };
                const float throughLength = std::sqrt(through.x * through.x + through.y * through.y + through.z * through.z);
                if ( throughLength < 1e-5f ) continue;
                through = { through.x / throughLength, through.y / throughLength, through.z / throughLength };
                // それぞれのレールの「始点→終点」の向きに直して渡す（終点側でつながる a は同じ向き、始点側なら逆。b はその反対）
                const float signA = aFront ? -1.0f : 1.0f;
                const float signB = bFront ? 1.0f : -1.0f;
                ( aFront ? a.hasFrontTangent : a.hasBackTangent ) = true;
                ( aFront ? a.frontTangent : a.backTangent ) = { through.x * signA, through.y * signA, through.z * signA };
                ( bFront ? b.hasFrontTangent : b.hasBackTangent ) = true;
                ( bFront ? b.frontTangent : b.backTangent ) = { through.x * signB, through.y * signB, through.z * signB };
                changed = true;
            }
        }
        return changed;
    }

    // フェーズ3.5：T字の端を本体へぴったり載せる（実行時のレールだけ。エディタのデータは変えない）。
    //   端がほかのレールの途中に kConnThreshold 以内で接しているのに、数cm〜数十cm離れていると、
    //   歩いて端から本体へ合流した瞬間にその分だけ横へ飛ぶ（乗り換えデバッガで 5〜35cm を測った）。
    //   端点を本体の上の一番近い点へ動かして、合流の瞬間に位置が飛ばないようにする。ノードを動かしたら true
    bool SnapEndsToBodies(std::vector<SplineRail>& rails){
        bool moved = false;
        for ( int j = 0; j < ( int ) rails.size(); ++j ) {
            SplineRail& rj = rails[j];
            if ( rj.nodes.size() < 2 || rj.isLoop || rj.HasMotion() ) continue;
            for ( int end = 0; end < 2; ++end ) {
                const bool isFront = ( end == 0 );
                if ( ( isFront ? rj.frontConnIndex : rj.backConnIndex ) >= 0 ) continue; // 端どうしでつながっている
                Vector3& node = isFront ? rj.nodes.front() : rj.nodes.back();
                for ( int i = 0; i < ( int ) rails.size(); ++i ) {
                    if ( i == j ) continue;
                    const SplineRail& ri = rails[i];
                    if ( ri.nodes.size() < 2 || ri.HasMotion() ) continue;
                    const float len = ri.GetLength();
                    if ( len < 1.5f ) continue;
                    const float cd = ri.GetClosestDistance(node);
                    if ( cd < 0.6f || cd > len - 0.6f ) continue; // 端付近は端点連結の領分（FindBranchPoints と同じ）
                    const Vector3 cp = ri.GetPositionByDistance(cd);
                    const float gap = VectorMath::Distance(cp, node);
                    if ( gap > kConnThreshold || gap < 1e-4f ) continue;
                    node = cp;
                    moved = true;
                    break;
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
                    if ( VectorMath::Distance(cp, e.pos) > kConnThreshold ) continue;

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
    bool hadGhost = false;
    for ( auto& rail : rails ) {
        rail.frontConnIndex = -1;
        rail.backConnIndex  = -1;
        rail.branchPoints.clear();
        hadGhost = hadGhost || rail.hasFrontTangent || rail.hasBackTangent;
        rail.hasFrontTangent = false;
        rail.hasBackTangent  = false;
    }
    if ( hadGhost ) { for ( auto& rail : rails ) { rail.BuildDistanceTable(); } } // 作り直し：前のつなぎ目の曲がりを消してから測る
    DetectLoops(rails);
    FindEndConnections(rails);
    const bool trimmed = TrimStubEnds(rails);
    // フェーズ3：ノードを動かしたので距離テーブルを作り直す
    const bool welded = WeldEnds(rails);
    // つなぎ目の向きをなめらかにつなぐ（溶接で点が動いた後に。曲線が変わるので距離の表も作り直す）
    const bool smoothed = SmoothWeldedEnds(rails);
    if ( welded || trimmed || smoothed ) {
        for ( auto& rail : rails ) { rail.BuildDistanceTable(); }
    }
    if ( SnapEndsToBodies(rails) ) {
        for ( auto& rail : rails ) { rail.BuildDistanceTable(); }
    }
    FindBranchPoints(rails);
}
