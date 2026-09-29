#include "game/stage/BlockShape.h"
#include "engine/rail/SplineRail.h"

#include <algorithm>
#include <cmath>

namespace {
    const float kVisualEmbed = 0.02f; // 見た目だけ道へ2cm沈める（スプライン曲率の継ぎ目で髪の毛ほど浮くのを隠す）
}

float PlacedBlock::SurfaceHeightAt(float atDist) const{
    float bottom = ( float ) level * BlockShape::kSize;
    if ( BlockShape::IsSlope(type) ) {
        float run = BlockShape::FootprintHalf(type) * 2.0f; // 45°=1m / 26°=2m
        float t = std::clamp(( atDist - ( dist - run * 0.5f ) ) / run, 0.0f, 1.0f);
        if ( ascend < 0 ) { t = 1.0f - t; }
        return bottom + t * BlockShape::kSize;
    }
    return bottom + BlockShape::kSize;
}

bool BlockShape::PoseOnRail(const std::vector<SplineRail>& rails, const PlacedBlock& block,
                            Vector3& outPos, float& outYaw, float* outPitch){
    if ( block.rail < 0 || block.rail >= ( int ) rails.size() ) return false;
    const SplineRail& rail = rails[block.rail];
    if ( rail.nodes.size() < 2 ) return false;

    float dist = std::clamp(block.dist, 0.0f, rail.GetLength());
    Vector3 base    = rail.GetPositionByDistance(dist);
    Vector3 tangent = rail.GetTangentByDistance(dist);

    // 道幅方向（水平の右）＝ up × tangent。接線がほぼ真上を向く場合はずらさない
    float horizLen = std::sqrt(tangent.x * tangent.x + tangent.z * tangent.z);
    Vector3 right { 0.0f, 0.0f, 0.0f };
    if ( horizLen > 1e-4f ) { right = { tangent.z / horizLen, 0.0f, -tangent.x / horizLen }; }

    // モデルは原点が底面中心・実寸1m角。底面は「道の上面」（＝レール線）＋段数に置く。
    //   これでブロックが道にぴったり接地し、プレイヤーが上に乗った時の見た目も
    //   道に立つ時と同じ基準になる（当たり判定はレール線基準のままで整合する）
    outPos = { base.x + right.x * block.side,
               base.y + kSurfaceY + ( float ) block.level * kSize - kVisualEmbed,
               base.z + right.z * block.side };
    outYaw = ( horizLen > 1e-4f ) ? std::atan2(tangent.x, tangent.z) : 0.0f;
    // 道の勾配（ピッチ）。道はレールに沿って傾くのに、ブロックを水平のまま置くと
    // 坂で縁が浮く/めり込む。この角度で傾ければ底面が道と平行になり、
    // 上面もレール相対で一定＝当たり判定(SurfaceHeightAt)と見た目が一致する
    if ( outPitch ) {
        *outPitch = ( horizLen > 1e-4f ) ? std::atan2(-tangent.y, horizLen) : 0.0f;
    }
    return true;
}
