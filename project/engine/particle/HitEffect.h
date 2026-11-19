#pragma once
#include "engine/3d/obj/Obj3d.h"
#include "engine/math/VectorMath.h"
#include "engine/math/struct.h"
#include <vector>
#include <memory>

namespace KazumimiEngine {

class Camera;

// =====================================================================
//  HitEffect：攻撃が当たった時の、火花 1 回分。
//   Initialize で火花を作り、Update で広がりながら消えていく。全部消えたら IsDead が true（持ち主が片付ける）
// =====================================================================
class HitEffect {
public:
    // 発生位置、カメラ、テクスチャ、環境マップを渡して初期化
    void Initialize(const Vector3& position, Camera* camera, uint32_t textureIndex, uint32_t envMapIndex);

    void Update();
    void Draw();

    // 全ての火花が消えたら true を返す（削除判定用）
    bool IsDead() const { return isDead_; }

private:
    // 火花 1 つ（見た目の物・寿命・今の時間・元の大きさ）
    struct Spark {
        std::unique_ptr<Obj3d> obj;
        float lifeTime;
        float currentTime;
        Vector3 baseScale;
    };

    std::vector<Spark> sparks_;
    bool isDead_ = false;
};

} // namespace KazumimiEngine
