#pragma once
#include "Collider.h"
#include "engine/utils/Level/LevelData.h"

// 当たり判定の計算アルゴリズムをまとめたクラス
class Collision {
public:
    

    //  球 と 球 の当たり判定
    static bool IsCollision(const Sphere& s1, const Sphere& s2);

    //  AABB と AABB の当たり判定
    static bool IsCollision(const AABB& aabb1, const AABB& aabb2);

    //  球 と AABB の当たり判定
    static bool IsCollision(const Sphere& sphere, const AABB& aabb);
	//  レイ と 球 の当たり判定
    static bool IsCollision(const Ray& ray, const Sphere& sphere);

    // 壁との当たり判定
    static bool CheckBlockCollision(const Vector3 &pos, float radius, const LevelData &level);
 
};