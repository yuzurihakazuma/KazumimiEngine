#include "game/rail/RailConnections.h"
#include "engine/math/VectorMath.h"
#include "engine/rail/SplineRail.h"

#include <algorithm>
#include <cmath>

using namespace KazumimiEngine;

namespace {
    const float kConnThreshold = 0.7f; // エディタの吸着半径と同じ（吸着済みノードは動かない）

    Vector3 MidPoint(const Vector3& a, const Vector3& b){
        return { ( a.x + b.x ) * 0.5f, ( a.y + b.y ) * 0.5f, ( a.z + b.z ) * 0.5f };
    }

    // フェーズ0：ループ（円状レール）の検出。front と back が近ければ閉じたレール
    void DetectLoops(std::vector<SplineRail>& rails){
        for ( auto& rail : rails ) {
            rail.SetIsLoop(false);
            if ( rail.Nodes().size() < 3 ) continue;
            if ( VectorMath::Distance(rail.Nodes().front(), rail.Nodes().back()) >= kConnThreshold ) continue;
            Vector3 mid = MidPoint(rail.Nodes().front(), rail.Nodes().back());
            rail.SetFrontNode(mid);
            rail.SetBackNode(mid);
            rail.SetIsLoop(true);
            rail.BuildDistanceTable();
        }
    }

    // フェーズ1：最も近い端点同士で連結関係を決める（動くレールは除外）
    void FindEndConnections(std::vector<SplineRail>& rails){
        for ( int i = 0; i < ( int ) rails.size(); ++i ) {
            if ( rails[i].Links().isLoop || rails[i].IsLinkExcluded() || rails[i].Nodes().size() < 2 ) continue;
            const Vector3& iFront = rails[i].Nodes().front();
            const Vector3& iBack  = rails[i].Nodes().back();
            float bestFront = kConnThreshold;
            float bestBack  = kConnThreshold;
            for ( int j = 0; j < ( int ) rails.size(); ++j ) {
                if ( i == j || rails[j].IsLinkExcluded() || rails[j].Nodes().size() < 2 ) continue;
                const Vector3& jFront = rails[j].Nodes().front();
                const Vector3& jBack  = rails[j].Nodes().back();

                float ff = VectorMath::Distance(iFront, jFront);
                float fb = VectorMath::Distance(iFront, jBack);
                if ( ff < bestFront ) { bestFront = ff; rails[i].SetFrontConnIndex(j); rails[i].SetFrontConnToFront(true); }
                if ( fb < bestFront ) { bestFront = fb; rails[i].SetFrontConnIndex(j); rails[i].SetFrontConnToFront(false); }

                float bf = VectorMath::Distance(iBack, jFront);
                float bb = VectorMath::Distance(iBack, jBack);
                if ( bf < bestBack ) { bestBack = bf; rails[i].SetBackConnIndex(j); rails[i].SetBackConnToFront(true); }
                if ( bb < bestBack ) { bestBack = bb; rails[i].SetBackConnIndex(j); rails[i].SetBackConnToFront(false); }
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
            if ( rails[i].Nodes().size() < 2 ) continue;
            for ( int end = 0; end < 2; ++end ) {
                const bool isFront = ( end == 0 );
                const int  j       = isFront ? rails[i].Links().frontConnIndex   : rails[i].Links().backConnIndex;
                const bool toFront = isFront ? rails[i].Links().frontConnToFront : rails[i].Links().backConnToFront;
                if ( j < 0 || j == i ) continue;
                const auto& nodes = rails[j].Nodes();
                const size_t n = nodes.size();
                if ( n < 3 ) continue;
                const Vector3& tip = isFront ? rails[i].Nodes().front() : rails[i].Nodes().back();
                const Vector3& jEnd = toFront ? nodes.front() : nodes.back();
                const Vector3& second = toFront ? nodes[1] : nodes[n - 2];
                const Vector3& third = toFront ? nodes[2] : nodes[n - 3];
                if ( VectorMath::Distance(tip, second) + 1e-4f >= VectorMath::Distance(tip, jEnd) ) continue;
                const float len = VectorMath::Distance(second, third);
                if ( len < 1e-4f ) continue;
                const Vector3 dir { ( second.x - third.x ) / len, ( second.y - third.y ) / len, ( second.z - third.z ) / len };
                const Vector3 pulled { second.x + dir.x * kKeep, second.y + dir.y * kKeep, second.z + dir.z * kKeep };
                if ( toFront ) { rails[j].SetFrontNode(pulled); } else { rails[j].SetBackNode(pulled); }
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
            if ( rails[i].Nodes().size() < 2 ) { orig[i].valid = false; continue; }
            orig[i] = { rails[i].Nodes().front(), rails[i].Nodes().back(), true };
        }
        // レール j の端(front/back)が「レール i の端」を選び返しているか
        auto mutual = [&](int i, bool iIsFront, int j, bool jEndIsFront) -> bool{
            int  jConn      = jEndIsFront ? rails[j].Links().frontConnIndex   : rails[j].Links().backConnIndex;
            bool jConnFront = jEndIsFront ? rails[j].Links().frontConnToFront : rails[j].Links().backConnToFront;
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
                    const int  j       = isFront ? rails[i].Links().frontConnIndex   : rails[i].Links().backConnIndex;
                    const bool toFront = isFront ? rails[i].Links().frontConnToFront : rails[i].Links().backConnToFront;
                    if ( j < 0 || !orig[j].valid ) continue;
                    if ( mutual(i, isFront, j, toFront) != wantMutual ) continue;
                    const Vector3 node = wantMutual
                        ? MidPoint(isFront ? orig[i].front : orig[i].back, toFront ? orig[j].front : orig[j].back)
                        : ( toFront ? rails[j].Nodes().front() : rails[j].Nodes().back() );
                    if ( isFront ) { rails[i].SetFrontNode(node); } else { rails[i].SetBackNode(node); }
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
        for ( auto& rail : rails ) { rail.SetHasFrontTangent(false); rail.SetHasBackTangent(false); }
        bool changed = false;
        for ( int i = 0; i < ( int ) rails.size(); ++i ) {
            SplineRail& a = rails[i];
            if ( a.Nodes().size() < 2 || a.Links().isLoop || a.IsLinkExcluded() || a.Config().lineMode != 0 ) continue;
            for ( int end = 0; end < 2; ++end ) {
                const bool aFront = ( end == 0 );
                const int  j      = aFront ? a.Links().frontConnIndex : a.Links().backConnIndex;
                if ( j <= i ) continue; // つながっていない・自分自身・相手の側で処理済み
                SplineRail& b = rails[j];
                if ( b.Nodes().size() < 2 || b.Links().isLoop || b.IsLinkExcluded() || b.Config().lineMode != 0 ) continue;
                const bool bFront = aFront ? a.Links().frontConnToFront : a.Links().backConnToFront;
                // 互いに選び合っている組だけ（片思いの端は、相手の曲線を変えない）
                const int  backIndex = bFront ? b.Links().frontConnIndex : b.Links().backConnIndex;
                const bool backFront = bFront ? b.Links().frontConnToFront : b.Links().backConnToFront;
                if ( backIndex != i || backFront != aFront ) continue;

                const Vector3& joint = aFront ? a.Nodes().front() : a.Nodes().back();
                const Vector3& aNext = aFront ? a.Nodes()[1] : a.Nodes()[a.Nodes().size() - 2]; // つなぎ目の次の点（自分の側）
                const Vector3& bNext = bFront ? b.Nodes()[1] : b.Nodes()[b.Nodes().size() - 2]; // つなぎ目の次の点（相手の側）
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
                const Vector3 tangentA { through.x * signA, through.y * signA, through.z * signA };
                const Vector3 tangentB { through.x * signB, through.y * signB, through.z * signB };
                if ( aFront ) { a.SetHasFrontTangent(true); a.SetFrontTangent(tangentA); } else { a.SetHasBackTangent(true); a.SetBackTangent(tangentA); }
                if ( bFront ) { b.SetHasFrontTangent(true); b.SetFrontTangent(tangentB); } else { b.SetHasBackTangent(true); b.SetBackTangent(tangentB); }
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
            if ( rj.Nodes().size() < 2 || rj.Links().isLoop || rj.IsLinkExcluded() ) continue;
            for ( int end = 0; end < 2; ++end ) {
                const bool isFront = ( end == 0 );
                if ( ( isFront ? rj.Links().frontConnIndex : rj.Links().backConnIndex ) >= 0 ) continue; // 端どうしでつながっている
                const Vector3 node = isFront ? rj.Nodes().front() : rj.Nodes().back();
                for ( int i = 0; i < ( int ) rails.size(); ++i ) {
                    if ( i == j ) continue;
                    const SplineRail& ri = rails[i];
                    if ( ri.Nodes().size() < 2 || ri.IsLinkExcluded() ) continue;
                    const float len = ri.GetLength();
                    if ( len < 1.5f ) continue;
                    const float cd = ri.GetClosestDistance(node);
                    if ( cd < 0.6f || cd > len - 0.6f ) continue; // 端付近は端点連結の領分（FindBranchPoints と同じ）
                    const Vector3 cp = ri.GetPositionByDistance(cd);
                    const float gap = VectorMath::Distance(cp, node);
                    if ( gap > kConnThreshold || gap < 1e-4f ) continue;
                    if ( isFront ) { rj.SetFrontNode(cp); } else { rj.SetBackNode(cp); }
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
            if ( rj.Nodes().size() < 2 || rj.Links().isLoop || rj.IsLinkExcluded() ) continue;

            struct EndInfo { Vector3 pos; bool isFront; int connIdx; };
            const EndInfo ends[2] = {
                { rj.Nodes().front(), true,  rj.Links().frontConnIndex },
                { rj.Nodes().back(),  false, rj.Links().backConnIndex  },
            };
            for ( const auto& e : ends ) {
                if ( e.connIdx >= 0 ) continue; // 端点同士で連結済みなら分岐扱いにしない
                for ( int i = 0; i < ( int ) rails.size(); ++i ) {
                    if ( i == j ) continue;
                    SplineRail& ri = rails[i];
                    if ( ri.Nodes().size() < 2 || ri.IsLinkExcluded() ) continue;
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
                    float cross = ( ri.Config().type == SplineRail::RailType::Horizontal ) ? ( into.z - cp.z )
                                                                                  : ( into.x - cp.x );
                    if ( std::abs(cross) < 0.3f ) continue; // 交差方向が曖昧（ほぼ平行）→キーに割当不能

                    SplineRail::BranchPoint bp;
                    bp.distance   = cd;
                    bp.targetRail = j;
                    bp.targetDist = e.isFront ? 0.0f : rj.GetLength();
                    bp.zSign      = ( cross >= 0.0f ) ? +1 : -1;
                    ri.AddBranchPoint(bp);
                    break; // この端は登録済み。他のレールへの多重登録はしない
                }
            }
        }
    }
}

void RailConnections::Build(std::vector<SplineRail>& rails){
    bool hadGhost = false;
    for ( auto& rail : rails ) {
        rail.SetFrontConnIndex(-1);
        rail.SetBackConnIndex(-1);
        rail.ClearBranchPoints();
        hadGhost = hadGhost || rail.Links().hasFrontTangent || rail.Links().hasBackTangent;
        rail.SetHasFrontTangent(false);
        rail.SetHasBackTangent(false);
    }
    if ( hadGhost ) { for ( auto& rail : rails ) { rail.RebuildIfDirty(); } } // 作り直し：前のつなぎ目の曲がりを消してから測る（曲がりを消したレールだけ）
    DetectLoops(rails);
    FindEndConnections(rails);
    const bool trimmed = TrimStubEnds(rails);
    // フェーズ3：ノードを動かしたので距離テーブルを作り直す
    const bool welded = WeldEnds(rails);
    // つなぎ目の向きをなめらかにつなぐ（溶接で点が動いた後に。曲線が変わるので距離の表も作り直す）
    const bool smoothed = SmoothWeldedEnds(rails);
    // 点や向きが変わったレールだけ作り直す（変わっていないレールの表は、作り直しても同じ物になる）
    if ( welded || trimmed || smoothed ) {
        for ( auto& rail : rails ) { rail.RebuildIfDirty(); }
    }
    if ( SnapEndsToBodies(rails) ) {
        for ( auto& rail : rails ) { rail.RebuildIfDirty(); }
    }
    FindBranchPoints(rails);
}
