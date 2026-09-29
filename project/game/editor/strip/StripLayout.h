#pragma once
// =====================================================================
//  StripLayout：展開図の寸法と座標の変換（このフレームの値。StripCanvas が描く前に決める）
//   レール上の距離・高さ ⇔ 画面のピクセル の変換をここにまとめ、描画と各道具が同じ式を使う。
//   マス k は距離 k を中心に前後 0.5m を占める（ブロックの置かれ方と同じ）
// =====================================================================
#include "game/editor/strip/StripCommon.h"

struct StripLayout {
    float originX = 0.0f, originY = 0.0f; // キャンバス左上（スクリーン座標）
    float cellPx  = 28.0f;                // 1m = 何ピクセルか
    float groundY = 0.0f;                 // 道の上面のスクリーンY
    float infoTop = 0.0f;                 // 下の情報帯（高さの変化・接続）の上端
    float width = 0.0f, height = 0.0f;    // キャンバス全体
    float railLength = 0.0f;
    int   lastCell = 0;                   // 置ける一番端のマス（距離）
    int   levels = 8;                     // 表示する段数

    float DistToX(float dist) const{
        return originX + railstrip::kPadLeft + ( dist + 0.5f ) * cellPx;
    }
    float HeightToY(float heightMeters) const{
        return groundY - heightMeters * cellPx;
    }
    float XToDist(float screenX) const{
        return ( screenX - originX - railstrip::kPadLeft ) / cellPx - 0.5f;
    }
    float YToHeight(float screenY) const{
        return ( groundY - screenY ) / cellPx;
    }
};
