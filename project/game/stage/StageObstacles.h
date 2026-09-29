#pragma once
// =====================================================================
//  StageObstacles：卵・吐き出し弾の「壁・地面」への当たり判定（ブロック＋道の上面）。
//   球が from→to へ動いた間に、ブロックか道へ触れたら true（outHitPos=当たる直前の位置）。
//   以前は「ワールドの高さ0」だけを地面にしていたため、ブロックは素通りし、
//   高い道から投げると道を突き抜け、低い道では投げた瞬間に割れていた。
//   EggSystem の SetObstacleQuery へ Sweep を渡して使う
// =====================================================================
#include "engine/math/struct.h"

#include <vector>

class BlockSystem;
class SplineRail;

class StageObstacles {
public:
    // 参照先（どちらもシーンが持つ。所有しない）
    void Initialize(const BlockSystem* blocks, const std::vector<SplineRail>* rails){
        blocks_ = blocks;
        rails_  = rails;
    }

    // 1) ブロック 2) 道の上面 の順に調べる
    bool Sweep(const Vector3& from, const Vector3& to, float radius, Vector3& outHitPos) const;

private:
    // 移動後の位置 to が、どれかの道の断面（幅×厚み）に入っているか
    bool HitRoadSurface(const Vector3& to, float radius, Vector3& outHitPos) const;

    const BlockSystem*             blocks_ = nullptr;
    const std::vector<SplineRail>* rails_  = nullptr;
};
