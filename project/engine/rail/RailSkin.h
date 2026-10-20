#pragma once
// =====================================================================
//  RailSkin：レール1本の「道の見た目」（布ロード企画書）。
//   レールは移動の骨格のまま残し、見た目だけをここで選ぶ（移動・当たり判定はレールで行う）。
//     0 = 土台のみ … 今までの草＋段ボールの土台
//     1 = 布のみ   … 地面（草原・段ボール床）の上に布を敷く
//     2 = 布＋土台 … 下に地面が無い所（宙・水の上）の足場。土台の上に布を敷く
//     3 = 見せない … カメラ用・演出用など、見た目を出さないレール（移動はできる）
//   保存は LevelData::railSkins（railLines と同じ並び）。マップ JSON の "railSkins"
// =====================================================================
#include "engine/math/struct.h"

struct RailSkin {
    enum Style { Base = 0, Cloth = 1, ClothOnBase = 2, Hidden = 3 };
    int     style = Base;
    float   width = 1.8f;                     // 布の幅(m)
    float   lift = 0.012f;                    // 地面（レール面）からの浮き(m)。Z ファイティング防止
    float   curl = 0.035f;                    // 端のめくれの高さ(m)
    Vector3 color { 0.95f, 0.55f, 0.28f };    // 布の色（sRGB 0〜1）
    int     stitch = 1;                       // 端のステッチ（縫い目）を出すか
    int     layer = 0;                        // 重なり順（つなぎ目で大きい方が上に乗る）

    bool HasCloth() const { return style == Cloth || style == ClothOnBase; }
    bool HasBase() const { return style == Base || style == ClothOnBase; }
    bool operator==(const RailSkin& o) const{
        return style == o.style && width == o.width && lift == o.lift && curl == o.curl
            && color.x == o.color.x && color.y == o.color.y && color.z == o.color.z
            && stitch == o.stitch && layer == o.layer;
    }
    bool operator!=(const RailSkin& o) const{ return !( *this == o ); }
};
