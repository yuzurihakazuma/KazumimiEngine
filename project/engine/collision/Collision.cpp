#include "Collision.h"
// --- 標準ライブラリ ---
#include <cmath>
#include <algorithm>

// 1. 球 と 球
bool Collision::IsCollision(const Sphere& s1, const Sphere& s2) {
    // 2点間の距離の2乗を求める
    float dx = s1.center.x - s2.center.x;
    float dy = s1.center.y - s2.center.y;
    float dz = s1.center.z - s2.center.z;
    float distSq = dx * dx + dy * dy + dz * dz;

    // 半径の合計の2乗を求める
    float radiusSum = s1.radius + s2.radius;
    float radiusSumSq = radiusSum * radiusSum;

    // 距離が半径の合計より小さければ当たっている！
    return distSq <= radiusSumSq;
}

// 2. AABB と AABB
bool Collision::IsCollision(const AABB& aabb1, const AABB& aabb2) {
    // 全ての軸(X, Y, Z)で重なっていれば衝突
    if (aabb1.min.x <= aabb2.max.x && aabb1.max.x >= aabb2.min.x &&
        aabb1.min.y <= aabb2.max.y && aabb1.max.y >= aabb2.min.y &&
        aabb1.min.z <= aabb2.max.z && aabb1.max.z >= aabb2.min.z) {
        return true;
    }
    return false;
}

// 3. 球 と AABB
bool Collision::IsCollision(const Sphere& sphere, const AABB& aabb) {
    // 球の中心に一番近いAABB上の点を求める（クランプ）
    Vector3 closestPoint;
    closestPoint.x = std::clamp(sphere.center.x, aabb.min.x, aabb.max.x);
    closestPoint.y = std::clamp(sphere.center.y, aabb.min.y, aabb.max.y);
    closestPoint.z = std::clamp(sphere.center.z, aabb.min.z, aabb.max.z);

    // その一番近い点と、球の中心との距離の2乗を求める
    float dx = closestPoint.x - sphere.center.x;
    float dy = closestPoint.y - sphere.center.y;
    float dz = closestPoint.z - sphere.center.z;
    float distSq = dx * dx + dy * dy + dz * dz;

    // 距離が球の半径の2乗より小さければ当たっている！
    return distSq <= (sphere.radius * sphere.radius);
}

// 4. レイ と 球
bool Collision::IsCollision(const Ray& ray, const Sphere& sphere) {
    // 球の中心からレイの始点へのベクトル
    Vector3 m = {
        ray.origin.x - sphere.center.x,
        ray.origin.y - sphere.center.y,
        ray.origin.z - sphere.center.z
    };

    // 二次方程式の係数を求める (aはdirectionが正規化されていれば1なので省略)
    // b = direction と m の内積
    float b = (ray.direction.x * m.x) + (ray.direction.y * m.y) + (ray.direction.z * m.z);
    // c = mの長さの2乗 - 半径の2乗
    float c = (m.x * m.x) + (m.y * m.y) + (m.z * m.z) - (sphere.radius * sphere.radius);

    // 判別式 D = b^2 - c
    float discriminant = b * b - c;

    // 判別式がマイナスなら、レイは球をかすりもしていない（外れ）
    if (discriminant < 0.0f) {
        return false;
    }

    // 当たっている場合、交点までの距離 t を求める
    float t1 = -b - std::sqrt(discriminant);
    float t2 = -b + std::sqrt(discriminant);

    // 交点がレイの射程距離(0 ～ length)の範囲内にあるかチェック
    if ((t1 >= 0.0f && t1 <= ray.length) || (t2 >= 0.0f && t2 <= ray.length)) {
        return true; // 射程内でヒット！
    }

    return false; // 射程外
}