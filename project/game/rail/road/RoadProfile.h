#pragma once
#include "engine/math/struct.h"
#include <cmath>

// =====================================================================
//  RoadProfile：道の断面プロファイルとアトラスUVの共有定義（道システム設計書 §4）。
//   ・掃引メッシュ（RoadSweepBuilder）とジャンクションパッチ（RoadJunctionPatchBuilder）が
//     同じ 上面/ベベル/壁/底 の寸法・UV帯を使うので、ここに一本化して継ぎ目を一致させる
//   ・道生成に固有の小さなベクトル補助（水平射影/左方向/小数部）もここに置く。
//     汎用の加減算/内積/長さ/正規化は VectorMath に一本化してある
// =====================================================================
namespace RoadProfile {

// --- 断面プロファイル（設計書 §4 の12頂点。lat=right方向, h=up方向）---
//   同じ位置でも法線/UVが違う点は別頂点（ハードエッジ）。
struct ProfileV { float lat, h, v, nr, nu; };
inline constexpr ProfileV kProfile[12] = {
    { -1.00f, 0.00f, 0.5352f, -1.00f, 0.00f }, // 0 左壁 下
    { -1.00f, 0.15f, 0.6992f, -1.00f, 0.00f }, // 1 左壁 上
    { -1.00f, 0.15f, 0.7070f, -0.64f, 0.77f }, // 2 左ベベル 下
    { -0.88f, 0.25f, 0.7422f, -0.64f, 0.77f }, // 3 左ベベル 上
    { -0.88f, 0.25f, 0.7500f,  0.00f, 1.00f }, // 4 上面 左（坂:0.0234 / 危険帯:0.2070）
    { +0.88f, 0.25f, 1.0000f,  0.00f, 1.00f }, // 5 上面 右（坂:0.2031 / 危険帯:0.2734）
    { +0.88f, 0.25f, 0.7422f,  0.64f, 0.77f }, // 6 右ベベル 上
    { +1.00f, 0.15f, 0.7070f,  0.64f, 0.77f }, // 7 右ベベル 下
    { +1.00f, 0.15f, 0.6992f,  1.00f, 0.00f }, // 8 右壁 上
    { +1.00f, 0.00f, 0.5352f,  1.00f, 0.00f }, // 9 右壁 下
    { -1.00f, 0.00f, 0.4727f,  0.00f, -1.00f },// 10 底 左
    { +1.00f, 0.00f, 0.5273f,  0.00f, -1.00f },// 11 底 右
};
inline constexpr int   kStrips[6][2] = { { 0,1 }, { 2,3 }, { 4,5 }, { 6,7 }, { 8,9 }, { 10,11 } };
inline constexpr float kSlopeV[2]  = { 0.0234f, 0.2031f }; // 坂帯の上面UV(v)。頂点4/5用
inline constexpr float kDangerV[2] = { 0.2070f, 0.2734f }; // 危険帯（穴の手前後）の上面UV(v)。アトラスv5

// 切り口フタ用：断面の外周をなす6頂点（プロファイル番号。凸六角形）
inline constexpr int kOutline[6] = { 0, 1, 3, 5, 7, 9 };

// --- 断面の寸法（ジャンクションパッチ用。上の表から取り出して掃引側と必ず一致させる）---
inline constexpr float kHalfWidth  = kProfile[9].lat; // 道の半幅 W
inline constexpr float kInnerWidth = kProfile[5].lat; // ベベル内側 W-B
inline constexpr float kTopH       = kProfile[4].h;   // 上面の断面高さ
inline constexpr float kBevelH     = kProfile[1].h;   // ベベル下端の断面高さ

// --- 断面の各帯のUV(v)と法線（パッチのベベル/壁/底も同じ帯を使う）---
inline constexpr float kBevelTopV    = kProfile[3].v;  // ベベル 上
inline constexpr float kBevelBottomV = kProfile[2].v;  // ベベル 下
inline constexpr float kWallTopV     = kProfile[1].v;  // 壁 上
inline constexpr float kWallBottomV  = kProfile[0].v;  // 壁 下
inline constexpr float kBottomV0     = kProfile[10].v; // 底面の帯（左端）
inline constexpr float kBottomV1     = kProfile[11].v; // 底面の帯（右端）
inline constexpr float kBevelNr      = kProfile[6].nr; // ベベル法線の外向き成分
inline constexpr float kBevelNu      = kProfile[6].nu; // ベベル法線の上向き成分

// --- 共通の生成パラメータ（設計書 §4 の確定値）---
inline constexpr float kTopOffset  = -0.25f;           // 断面高さへの加算。上面(0.25)がレール線ぴったりに来る
                                                       // （レール線＝歩行面＝ブロックの底面基準。ズレ補正が要らなくなる）
inline constexpr float kUvPerMeter = 1.0f / 2.0f;      // u = 距離 / 2m（ピースとテクスチャ周期を共有）
inline constexpr float kDarkV      = 0.465f;           // アトラスの黒帯の中央（穴の奈落フタ用）
inline constexpr float kPi         = 3.14159265f;

// 水平（XZ）へ射影して正規化。ほぼ真上/真下なら false
inline bool ToHorizontal(const Vector3& v, Vector3& out){
    float l = std::sqrt(v.x * v.x + v.z * v.z);
    if ( l < 0.35f ) return false; // 急勾配すぎてジャンクションの水平方向が定まらない
    out = { v.x / l, 0.0f, v.z / l };
    return true;
}
// 水平方向 d の「左」（角度ソートのCCWと整合する側）
inline Vector3 LeftOf(const Vector3& d){ return { -d.z, 0.0f, d.x }; }
inline float   Fract(float x){ return x - std::floor(x); }

} // namespace RoadProfile
