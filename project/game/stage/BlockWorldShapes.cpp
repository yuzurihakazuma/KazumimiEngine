#include "game/stage/BlockWorldShapes.h"
#include "engine/graphics/DebugDraw.h"

#include <algorithm>
#include <cmath>

bool BlockWorldShapes::MakeBox(const std::vector<SplineRail>& rails, const PlacedBlock& block, Box& box){
    Vector3 origin {}; float yaw = 0.0f; float pitch = 0.0f;
    if ( !BlockShape::PoseOnRail(rails, block, origin, yaw, &pitch) ) return false;

    // 描画の回転（Rx(pitch)→Ry(yaw)）と同じ向きの3軸
    float sinYaw = std::sin(yaw), cosYaw = std::cos(yaw);
    float sinPitch = std::sin(pitch), cosPitch = std::cos(pitch);
    box.right   = { cosYaw, 0.0f, -sinYaw };
    box.up      = { sinPitch * sinYaw, cosPitch, sinPitch * cosYaw };
    box.forward = { sinYaw * cosPitch, -sinPitch, cosYaw * cosPitch };

    // 寸法はモデルの実寸：斜面と台座は道幅2m、横長・台座・ゆるい斜面は進行方向2m
    const float kHalf = BlockShape::kHalf;
    float halfSide   = ( BlockShape::IsSlope(block.type) || block.type == BlockShape::kTypePedestal ) ? 1.0f : kHalf;
    float halfAlong  = BlockShape::FootprintHalf(block.type);
    float halfHeight = kHalf;
    float centerUp   = kHalf; // 底面中心（原点）から箱の中心までの高さ
    if ( block.type == BlockShape::kTypeCloud ) { centerUp = 0.85f; halfHeight = 0.15f; } // セル上端の薄い板
    box.half   = { halfSide, halfHeight, halfAlong };
    box.center = { origin.x + box.up.x * centerUp,
                   origin.y + box.up.y * centerUp,
                   origin.z + box.up.z * centerUp };
    box.slope  = BlockShape::IsSlope(block.type);
    box.ascend = block.ascend;
    return true;
}

void BlockWorldShapes::Rebuild(const std::vector<PlacedBlock>& blocks, const std::vector<SplineRail>& rails){
    boxes_.clear();
    boxes_.reserve(blocks.size());
    for ( int i = 0; i < ( int ) blocks.size(); ++i ) {
        Box box;
        if ( !MakeBox(rails, blocks[i], box) ) continue;
        box.blockIndex = i;
        boxes_.push_back(box);
    }
}

// 点と箱の距離の2乗（箱のローカル座標へ直してから、箱の中の一番近い点までを測る）
float BlockWorldShapes::DistSqToBox(const Box& box, const Vector3& worldPoint){
    Vector3 delta = { worldPoint.x - box.center.x, worldPoint.y - box.center.y, worldPoint.z - box.center.z };
    float localX = delta.x * box.right.x   + delta.y * box.right.y   + delta.z * box.right.z;
    float localY = delta.x * box.up.x      + delta.y * box.up.y      + delta.z * box.up.z;
    float localZ = delta.x * box.forward.x + delta.y * box.forward.y + delta.z * box.forward.z;

    float nearX = std::clamp(localX, -box.half.x, box.half.x);
    float nearY = std::clamp(localY, -box.half.y, box.half.y);
    float nearZ = std::clamp(localZ, -box.half.z, box.half.z);
    if ( box.slope ) {
        // 斜面：上面は進行方向へ直線的に高くなる。坂の上の空間は箱の外として扱う
        float t = ( nearZ + box.half.z ) / ( box.half.z * 2.0f );
        if ( box.ascend < 0 ) { t = 1.0f - t; }
        float surfaceY = -box.half.y + t * box.half.y * 2.0f;
        nearY = ( std::min )( nearY, surfaceY );
    }
    float dx = localX - nearX, dy = localY - nearY, dz = localZ - nearZ;
    return dx * dx + dy * dy + dz * dz;
}

bool BlockWorldShapes::SweepSphere(const Vector3& from, const Vector3& to, float radius, Vector3* outHitPos) const{
    if ( boxes_.empty() ) return false;
    Vector3 delta = { to.x - from.x, to.y - from.y, to.z - from.z };
    float length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    Vector3 middle = { from.x + delta.x * 0.5f, from.y + delta.y * 0.5f, from.z + delta.z * 0.5f };

    // 移動区間に届きうる箱だけを先に拾う（箱の外接球 ＋ 区間の半分 ＋ 球の半径）
    const float kBoxBound = 1.75f; // 一番大きい箱（2×1×2m）の対角の半分より少し大きい値
    float reach = length * 0.5f + radius + kBoxBound;
    std::vector<const Box*> candidates;
    for ( const Box& box : boxes_ ) {
        float dx = box.center.x - middle.x, dy = box.center.y - middle.y, dz = box.center.z - middle.z;
        if ( dx * dx + dy * dy + dz * dz <= reach * reach ) { candidates.push_back(&box); }
    }
    if ( candidates.empty() ) return false;

    // 区間を球の半径の半分刻みで進めて調べる（速い弾が薄い壁を1フレームで飛び越さない）
    int steps = std::clamp(( int ) std::ceil(length / ( std::max )( radius * 0.5f, 0.05f )), 1, 96);
    Vector3 lastFree = from;
    for ( int i = 1; i <= steps; ++i ) {
        float t = ( float ) i / ( float ) steps;
        Vector3 point = { from.x + delta.x * t, from.y + delta.y * t, from.z + delta.z * t };
        for ( const Box* box : candidates ) {
            if ( DistSqToBox(*box, point) <= radius * radius ) {
                if ( outHitPos ) { *outHitPos = lastFree; }
                return true;
            }
        }
        lastFree = point;
    }
    return false;
}

bool BlockWorldShapes::Raycast(const Vector3& origin, const Vector3& direction, Hit& outHit) const{
    bool found = false;
    float bestDistance = 1e9f;
    for ( const Box& box : boxes_ ) {
        // レイを箱のローカル座標へ（3軸それぞれの板の間に入る区間の重なり＝スラブ法）
        Vector3 delta = { origin.x - box.center.x, origin.y - box.center.y, origin.z - box.center.z };
        const Vector3* axes[3] = { &box.right, &box.up, &box.forward };
        const float halves[3] = { box.half.x, box.half.y, box.half.z };
        float tEnter = 0.0f, tExit = 1e9f;
        int   enterAxis = 1, enterSign = 1;
        bool  miss = false;
        for ( int axis = 0; axis < 3 && !miss; ++axis ) {
            float localOrigin = delta.x * axes[axis]->x + delta.y * axes[axis]->y + delta.z * axes[axis]->z;
            float localDir    = direction.x * axes[axis]->x + direction.y * axes[axis]->y + direction.z * axes[axis]->z;
            if ( std::abs(localDir) < 1e-6f ) {
                if ( std::abs(localOrigin) > halves[axis] ) { miss = true; }
                continue;
            }
            float t0 = ( -halves[axis] - localOrigin ) / localDir;
            float t1 = (  halves[axis] - localOrigin ) / localDir;
            int   sign = -1; // t0 は「−側の面」から入る場合
            if ( t0 > t1 ) { std::swap(t0, t1); sign = 1; }
            if ( t0 > tEnter ) { tEnter = t0; enterAxis = axis; enterSign = sign; }
            if ( t1 < tExit )  { tExit = t1; }
            if ( tEnter > tExit ) { miss = true; }
        }
        if ( miss || tExit < 0.0f ) continue;
        if ( tEnter < bestDistance ) {
            bestDistance = tEnter;
            outHit = { box.blockIndex, tEnter, enterAxis, enterSign, box.half.z, box.half.x };
            found = true;
        }
    }
    return found;
}

void BlockWorldShapes::DrawWire(const Box& box, const Vector4& color, float inflate){
    DebugDraw* debugDraw = DebugDraw::GetInstance();
    // 8頂点（ビット0=道幅 / ビット1=上下 / ビット2=進行方向）
    Vector3 corners[8];
    for ( int i = 0; i < 8; ++i ) {
        float signX = ( i & 1 ) ? 1.0f : -1.0f;
        float signY = ( i & 2 ) ? 1.0f : -1.0f;
        float signZ = ( i & 4 ) ? 1.0f : -1.0f;
        float heightScale = 1.0f;
        if ( box.slope && signY > 0.0f ) {
            // 斜面の上面：低い側の縁は底面と同じ高さまで下げる
            bool highEdge = ( signZ > 0.0f ) == ( box.ascend > 0 );
            if ( !highEdge ) { heightScale = -1.0f; }
        }
        float offsetX = signX * ( box.half.x + inflate );
        float offsetY = signY * ( box.half.y + inflate ) * heightScale;
        float offsetZ = signZ * ( box.half.z + inflate );
        corners[i] = {
            box.center.x + box.right.x * offsetX + box.up.x * offsetY + box.forward.x * offsetZ,
            box.center.y + box.right.y * offsetX + box.up.y * offsetY + box.forward.y * offsetZ,
            box.center.z + box.right.z * offsetX + box.up.z * offsetY + box.forward.z * offsetZ };
    }
    const int edges[12][2] = {
        { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 },   // 道幅方向
        { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 },   // 上下
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } }; // 進行方向
    for ( const auto& edge : edges ) { debugDraw->Line(corners[edge[0]], corners[edge[1]], color); }
}

void BlockWorldShapes::DrawAll(const Vector4& color) const{
    for ( const Box& box : boxes_ ) { DrawWire(box, color, 0.0f); }
}
