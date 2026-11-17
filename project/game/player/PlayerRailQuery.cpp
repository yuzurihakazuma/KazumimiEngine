#include "game/player/PlayerRailQuery.h"
#include "engine/rail/SplineRail.h"
#include "engine/math/VectorMath.h"

#include <algorithm>
#include <cmath>

using namespace VectorMath;

namespace {
    // 水平(x,z)を単位ベクトル化（長さ0なら0ベクトル）
    Vector3 HorizDir(float x, float z){
        float len = std::sqrt(x * x + z * z);
        if ( len < 1e-4f ) return { 0.0f, 0.0f, 0.0f };
        return { x / len, 0.0f, z / len };
    }
}

namespace PlayerRailQuery {

bool IsOverHole(const std::vector<SplineRail>& rails, const Vector3& footPos){
    for ( const SplineRail& rail : rails ) {
        if ( rail.nodes.size() < 2 || rail.nodeHole.empty() ) continue;
        if ( rail.IsRideBlocked() ) continue; // 出現前の道の穴では落ちない
        if ( rail.isTrack ) continue;         // 線路（歩かない）の穴では落ちない
        float closestDist = rail.GetClosestDistance(footPos);
        if ( !rail.IsHoleAtDistance(closestDist) ) continue;
        Vector3 closestPos = rail.GetPositionByDistance(closestDist);
        float dx = closestPos.x - footPos.x, dz = closestPos.z - footPos.z;
        // 高さ許容は±0.35m（広すぎると上下に重なった別の階の穴に誤爆して落ちる）
        if ( std::sqrt(dx * dx + dz * dz) < 0.5f && std::abs(closestPos.y - footPos.y) < 0.35f ) return true;
    }
    return false;
}

bool FindJoin(const std::vector<SplineRail>& rails, int currentRail,
              const Vector3& edgePos, const Vector3& forwardDir, Spot& out){
    const float kJoinReach = 1.2f;
    float bestDist = kJoinReach;
    bool found = false;
    for ( int j = 0; j < ( int ) rails.size(); ++j ) {
        if ( j == currentRail ) continue;
        const SplineRail& candidateRail = rails[j];
        if ( candidateRail.nodes.size() < 2 ) continue;
        if ( candidateRail.IsRideBlocked() ) continue; // まだ出現していない道へは合流しない
        if ( candidateRail.IsUnwalkable() ) continue;  // 線路・走っている列車へは合流しない（列車の足場の端から線路へ降りていた）
        float closestDist = candidateRail.GetClosestDistance(edgePos);
        Vector3 closestPos = candidateRail.GetPositionByDistance(closestDist);
        float dx = closestPos.x - edgePos.x, dy = closestPos.y - edgePos.y, dz = closestPos.z - edgePos.z;
        float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if ( distance >= bestDist ) continue;

        // 前方チェック：合流先が真横〜後方なら弾く（端点の真上を通る動くレール等、ほぼ同位置は許可）。
        //   これが無いと、1.2m以内を平行に走る隣のレールへ端に来ただけで勝手に飛び移る誤爆が起きる
        //   端が相手の上に乗っている（横に kOnTop 以内）なら向きは見ない：最寄り点は距離の表の刻み（数cm）でしか求まらず、
        //   真上に乗っていても数cm横にずれて「真横」と判定され、動く足場（列車など。端の寄せをしない）へ乗れなかった
        constexpr float kOnTop = 0.25f;
        const bool onTop = std::sqrt(dx * dx + dz * dz) <= kOnTop && std::abs(dy) < 0.5f; // 上下に重なった別の階は「乗っている」に入れない
        if ( Length(forwardDir) > 1e-4f && !onTop ) {
            Vector3 toTarget = HorizDir(dx, dz);
            if ( Length(toTarget) > 1e-4f && ( toTarget.x * forwardDir.x + toTarget.z * forwardDir.z ) < 0.1f ) continue;
        }
        // 乗り移りのスナップは見た目の平滑化で滑らかに補間するので、動くレールも 1.2m で確実に乗れる
        bestDist = distance;
        out = { j, closestDist, closestPos };
        found = true;
    }
    return found;
}

bool FindSwitch(const std::vector<SplineRail>& rails, int currentRail, const Vector3& footPos,
                bool currentHorizontal, int switchDir, Spot& out){
    // 止まっている列車から駅の道へ降りる時だけ広く拾う（台車の上でぴったり道の前に立たなくても降りられる。
    //   駅の道は台車の真ん中の線まで重ねて置くので、台車の上なら 1.2m 以内に必ず道のはしがある）
    const bool  fromTrain = currentRail >= 0 && currentRail < ( int ) rails.size() && rails[currentRail].IsTrain();
    const float kReach   = fromTrain ? 1.2f : 0.9f;  // 乗り換え先の最寄り点までの最大3D距離（狭いほど誤爆しない）
    const float kLateral = fromTrain ? 1.2f : 0.5f;  // 進行軸(横=X/縦=Z)の横ズレ上限。真上で交差してる相手だけ拾う
    const float kMinOff  = 0.3f;  // 押した方向にこれ以上伸びているレールであること
    const bool  wantHorizontalTarget = !currentHorizontal; // 縦に乗ってたら横へ／横なら縦へ
    const float myAxis = currentHorizontal ? footPos.z : footPos.x; // 横=Z(奥/手前) / 縦=X(右/左)

    float bestScore = 1e30f;
    bool found = false;
    for ( int j = 0; j < ( int ) rails.size(); ++j ) {
        if ( j == currentRail ) continue;
        const SplineRail& candidateRail = rails[j];
        if ( candidateRail.nodes.size() < 2 ) continue;
        if ( !candidateRail.visible ) continue; // 見えない連結レールへは乗り換えできない（見えない道を歩く混乱防止）
        if ( candidateRail.IsRideBlocked() ) continue; // まだ出現していない道へも乗り換えできない
        if ( candidateRail.IsUnwalkable() ) continue;  // 線路・走っている列車へも乗り換えできない
        if ( ( candidateRail.type == SplineRail::RailType::Horizontal ) != wantHorizontalTarget ) continue; // 反対タイプのみ

        float closestDist = candidateRail.GetClosestDistance(footPos);
        Vector3 closestPos = candidateRail.GetPositionByDistance(closestDist);
        float dx = closestPos.x - footPos.x, dy = closestPos.y - footPos.y, dz = closestPos.z - footPos.z;
        float dist3d = std::sqrt(dx * dx + dy * dy + dz * dz);
        if ( dist3d > kReach ) continue; // 遠いレールへは飛ばない

        float lateral = currentHorizontal ? std::abs(dx) : std::abs(dz); // 真上で交差してる相手だけ
        if ( lateral > kLateral ) continue;

        float axisMin = 1e30f, axisMax = -1e30f; // 押した方向に伸びているか
        for ( const auto& node : candidateRail.nodes ) {
            float axisValue = currentHorizontal ? node.z : node.x;
            axisMin = ( std::min )( axisMin, axisValue );
            axisMax = ( std::max )( axisMax, axisValue );
        }
        if ( switchDir > 0 ) { if ( axisMax < myAxis + kMinOff ) continue; }
        else                 { if ( axisMin > myAxis - kMinOff ) continue; }

        if ( dist3d < bestScore ) {
            bestScore = dist3d;
            out = { j, closestDist, closestPos };
            found = true;
        }
    }
    return found;
}

bool FindBranch(const std::vector<SplineRail>& rails, int currentRail, float currentDist,
                int switchDir, Spot& out){
    const float kNear = 0.9f; // 分岐点に反応する距離
    for ( const auto& branchPoint : rails[currentRail].branchPoints ) {
        if ( std::abs(currentDist - branchPoint.distance) > kNear ) continue;
        if ( ( switchDir > 0 ) != ( branchPoint.zSign > 0 ) ) continue; // 押した向きと分岐の向きが一致する時だけ
        if ( branchPoint.targetRail < 0 || branchPoint.targetRail >= ( int ) rails.size() ) continue;
        const SplineRail& targetRail = rails[branchPoint.targetRail];
        if ( targetRail.GetLength() <= 0.0f ) continue;
        if ( targetRail.IsRideBlocked() ) continue; // まだ出現していない道へは分岐できない
        out = { branchPoint.targetRail, branchPoint.targetDist, targetRail.GetPositionByDistance(branchPoint.targetDist) };
        return true;
    }
    return false;
}

bool FindRailBelow(const std::vector<SplineRail>& rails, const Vector3& pos, Spot& out){
    const float kReachXZ = 0.8f; // 着地と同じ「レールの真上」の幅
    const float kAboveMax = 1.0f; // 足よりこれ以上高いレールは対象外（頭の上の別の階）
    float best = 1e9f;
    bool found = false;
    for ( int i = 0; i < ( int ) rails.size(); ++i ) {
        const SplineRail& rail = rails[i];
        if ( rail.nodes.size() < 2 || !rail.visible || rail.IsRideBlocked() || rail.IsUnwalkable() ) continue;
        float closestDist = rail.GetClosestDistance(pos);
        Vector3 closestPos = rail.GetPositionByDistance(closestDist);
        float dx = closestPos.x - pos.x, dz = closestPos.z - pos.z;
        if ( std::sqrt(dx * dx + dz * dz) > kReachXZ ) continue;
        if ( closestPos.y > pos.y + kAboveMax ) continue;
        // 上下に重なったレールでは、高さが一番近いもの（＝足の下の階）を選ぶ
        float vertical = std::abs(pos.y - closestPos.y);
        if ( vertical >= best ) continue;
        best = vertical;
        out = { i, closestDist, closestPos };
        found = true;
    }
    return found;
}

bool FindLanding(const std::vector<SplineRail>& rails, const Vector3& pos, float prevY,
                 int ignoreRail, Spot& out){
    const float kLandXZ = 0.8f; // 水平にこの距離以内なら「レールの真上」とみなす
    for ( int i = 0; i < ( int ) rails.size(); ++i ) {
        if ( i == ignoreRail ) continue;
        const SplineRail& rail = rails[i];
        if ( rail.nodes.size() < 2 ) continue;
        if ( !rail.visible ) continue; // 見えない連結レールには着地しない（床ではなく「道」なので）
        if ( rail.IsRideBlocked() ) continue; // まだ出現していない道にも着地しない（すり抜けて落ちる）
        if ( rail.IsUnwalkable() ) continue;  // 線路・走っている列車にも着地しない（列車は駅で止まっている時だけ乗る）

        float closestDist = rail.GetClosestDistance(pos);
        Vector3 closestPos = rail.GetPositionByDistance(closestDist);

        // 穴区間には着地しない（飛び越え中に穴の上で着地→即落下のループを防ぐ）
        if ( rail.IsHoleAtDistance(closestDist) ) continue;

        // 水平にレールの真上にいるか
        float dx = closestPos.x - pos.x, dz = closestPos.z - pos.z;
        if ( std::sqrt(dx * dx + dz * dz) > kLandXZ ) continue;

        // 縦：レール面に「降りてきて到達した」時だけ着地する（落下を最後まで見せる）。
        //   ・reached … レール面のすぐ近く(上0.1m〜下0.3m)に降りてきた＝自然な接地
        //   ・crossed … 高速落下で1フレームに面を上→下へ通過してもすり抜けずに拾う
        //   まだ上にいる間（above>0.1）は着地させない＝瞬間移動にならない
        float above   = pos.y - closestPos.y;
        bool  reached = ( above <= 0.1f && above >= -0.3f );
        bool  crossed = ( prevY >= closestPos.y && pos.y <= closestPos.y );
        if ( !reached && !crossed ) continue;

        out = { i, closestDist, closestPos };
        return true;
    }
    return false;
}

} // namespace PlayerRailQuery
