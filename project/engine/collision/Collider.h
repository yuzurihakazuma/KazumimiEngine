#pragma once
#include"engine/math/struct.h"

// 軸に平行な箱 
struct AABB {
    Vector3 min; // 最小座標 (左下奥)
    Vector3 max; // 最大座標 (右上隅)
};

struct Ray {
    Vector3 origin;    // 始点（発射位置）
    Vector3 direction; // 方向（※必ず正規化されたベクトルを入れる）
    float length;      // 届く距離（射程）
};