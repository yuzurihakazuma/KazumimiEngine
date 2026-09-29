#pragma once
// =====================================================================
//  BlockShape：ブロックの形の決まり（種類ごとの寸法）と、レール上の置き方。
//   ブロックは「レール番号・距離・段・横ずれ」で置く（レール空間）。
//   当たり判定（BlockSystem）・描画（BlockRenderer）・ワールドの箱（BlockWorldShapes）が
//   同じ寸法と同じ置き方を使うように、ここに1つだけ持つ（食い違うと「見えるのに当たらない」が起きる）
// =====================================================================
#include "engine/math/struct.h"

#include <vector>

class SplineRail;

namespace BlockShape {
    constexpr float kSize = 1.0f;          // 1ブロック = 1m角
    constexpr float kHalf = kSize * 0.5f;  // ブロック半幅 (0.5m)
    // 道の上面の高さ（レール線からの差）。RoadMesh 側を「上面＝レール線ぴったり」に
    // 揃えたので 0（レール線＝歩行面＝ブロック底面。全システムで基準が一致する）
    constexpr float kSurfaceY = 0.0f;

    constexpr int kTypeCount    = 9; // ブロックの種類数（BlockData::type と対応）
    constexpr int kTypeSpring   = 4; // ジャンプ台（上に飛び乗ると大きく跳ねる）
    constexpr int kTypeHatena   = 5; // ？ブロック（下から頭突きするとコインが出る。1回きり）
    constexpr int kTypeCloud    = 6; // すり抜け床（上には乗れる/下と横からは通り抜ける）
    constexpr int kTypeWide     = 7; // 横長ブロック（進行方向2m。長い足場を1個で作れる）
    constexpr int kTypePedestal = 8; // 台座ブロック（2×2m。道幅いっぱいの土台）

    // 斜面（2=45° / 3=ゆるい26°）か
    inline bool IsSlope(int type){ return type == 2 || type == 3; }
    // 進行方向の半幅（斜面26°・横長・台座は2mぶん＝1.0、他は0.5）
    inline float FootprintHalf(int type){
        return ( type == 3 || type == kTypeWide || type == kTypePedestal ) ? 1.0f : 0.5f;
    }
}

// 置かれたブロック1個（実行時）
struct PlacedBlock {
    int   rail  = 0;
    float dist  = 0.0f; // レール上の距離(m)。1m刻み
    int   level = 0;    // 段数（1段=1m）
    float side  = 0.0f; // 道幅方向のずれ(m)。0=中心線
    int   type  = 0;    // 見た目の種類（0=スポンジ/1=段ボール層/2=斜面45°/3=ゆるい斜面/4=バネ/5=？/6=すり抜け/7=横長2m/8=台座2×2m）
    int   ascend = 1;   // 斜面の登り方向（+1=dist増加側が高い/-1=逆。隣のブロックから自動決定）
    bool  used  = false; // ？ブロック：このPlayで既にコインを出したか

    // dist 位置での表面高さ（レール面からの相対）。矩形は上面一定、斜面はセル内で線形に上がる
    float SurfaceHeightAt(float atDist) const;
};

namespace BlockShape {
    // ブロックの基準位置（底面中心）・向き(yaw)・道の勾配(pitch) をワールドで求める。
    //   動くレールの現在位置（animOffset込み）を反映するので、リフト上のブロックも一緒に動く。
    //   レールが無効なら false
    bool PoseOnRail(const std::vector<SplineRail>& rails, const PlacedBlock& block,
                    Vector3& outPos, float& outYaw, float* outPitch = nullptr);
}
