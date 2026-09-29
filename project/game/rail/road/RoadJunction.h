#pragma once
#include "engine/math/struct.h"
#include "game/rail/road/RoadProfile.h"
#include <cmath>
#include <vector>

// =====================================================================
//  RoadJunction：ジャンクション（共有ノード）の共有データ型と幾何補助（GUIDE_ジャンクション生成）。
//   ・RoadJunctionDetector が検出して t_cut を決め、RoadJunctionPatchBuilder がパッチを張り、
//     RoadMesh がジョイントを置く。三者が同じ Arm/Junction を受け渡すためここに置く
//   ・RoadCut は「掃引をスキップする区間」。ジャンクション側が登録し、掃引側が面を張らない
// =====================================================================

// 掃引をスキップする区間（ジャンクションパッチに譲る範囲）
struct RoadCut { float s0, s1; };

// ジャンクション（共有ノード）の1本ぶんの腕
struct RoadArm {
    int     rail = -1;
    float   nodeS = 0.0f;   // ノードのレール距離
    float   cutS = 0.0f;    // 道を切るレール距離（= 入口リングの位置）
    bool    forward = true; // true = ノードから +s 方向へ伸びる腕
    Vector3 dir {};         // ノードから出ていく方向（水平・正規化）
};

struct RoadJunction {
    Vector3 center {};
    int     followRail = -1; // 動くレール追従用
    std::vector<RoadArm> arms;
};

namespace RoadJunctionGeom {

inline constexpr float kTCutMin     = 0.35f;   // t_cut の最低値
inline constexpr float kMiterMaxDeg = 165.0f;  // これ未満はマイター（165°〜195°は「ほぼ直進」でパッチ不要）

// マイター交点：N + t*di + w*Li = N + s*dj - w*Lj を XZ で解く（t, s を返す）
inline bool SolveMiter(const Vector3& di, const Vector3& dj, float w, float& t, float& s){
    Vector3 Li = RoadProfile::LeftOf(di), Lj = RoadProfile::LeftOf(dj);
    float bx = -w * ( Li.x + Lj.x );
    float bz = -w * ( Li.z + Lj.z );
    float det = di.x * ( -dj.z ) - ( -dj.x ) * di.z;
    if ( std::abs(det) < 1e-4f ) return false;
    t = ( bx * ( -dj.z ) - ( -dj.x ) * bz ) / det;
    s = ( di.x * bz - di.z * bx ) / det;
    return true;
}

// ウェッジ（腕 di → 次の腕 dj、CCW）の開き角(度)。(0, 360] で返す
//   t_cut 計算とパッチのウェッジ形状判定の両方が同じ角度で判断するため共通化
inline float WedgeAngleDeg(const Vector3& di, const Vector3& dj){
    float angI = std::atan2(di.z, di.x);
    float angJ = std::atan2(dj.z, dj.x);
    float phi = angJ - angI;
    while ( phi <= 0.0f ) { phi += 2.0f * RoadProfile::kPi; }
    return phi * 180.0f / RoadProfile::kPi;
}

} // namespace RoadJunctionGeom
