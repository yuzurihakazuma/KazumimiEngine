#include "game/enemy/EnemyBehavior.h"
#include "game/enemy/Enemy.h"
#include "game/combat/EnemyHit.h"

#include "engine/3d/obj/SkinnedObj3d.h"

#include <cmath>

using namespace KazumimiEngine;

namespace {
    constexpr float kBiteTime = 1.5f;        // 噛みつきの長さ(秒)：Bite（24 フレーム）＋ひと呼吸
    constexpr float kBiteHeightLimit = 2.0f; // 上下にこれ以上離れたプレイヤーには噛みつかない(m)
    constexpr float kFaceExtraRange = 1.4f;  // 噛みつく距離より、これだけ遠くから体を向ける(m)
    constexpr float kMinFaceDistance = 1e-4f;

    // 地上「ドングリン」：歩いている間は Walk、止まっている間は Idle
    class WalkerBehavior : public EnemyBehavior {
    public:
        const char* StartClip(bool patrols) const override{ return patrols ? "Walk" : "Idle"; }
        bool Update(SkinnedObj3d& model, const Frame& frame, float& /*outYaw*/) override{
            // SetClip は同じ名前なら何もしないので、毎フレーム呼んでよい。
            //   エディット中（時間が止まっている）は、動く設定の敵を Walk で見せる
            const bool walk = ( frame.deltaTime > 0.0f ) ? frame.moving : frame.canMove;
            model.SetClip(walk ? "Walk" : "Idle", true);
            return false;
        }
        void ReceiveHit(const EnemyHit& hit, Enemy& enemy, EnemyHitContext& context) const override{ hit.OnWalker(enemy, context); }
    };

    // 植物「カミバナ」：プレイヤーが近づくと噛みつく（終わったら Idle へ戻る）。近くでは体も向ける
    class BiterBehavior : public EnemyBehavior {
    public:
        const char* StartClip(bool /*patrols*/) const override{ return "Idle"; }
        void Reset() override{ biteTimer_ = 0.0f; }
        void OnReapplied(SkinnedObj3d& model) override{ model.SetClip("Idle", true); }
        bool Update(SkinnedObj3d& model, const Frame& frame, float& outYaw) override{
            if ( frame.deltaTime <= 0.0f ) { return false; } // エディット中は動かさない
            const float dx = frame.playerPos.x - frame.position.x;
            const float dz = frame.playerPos.z - frame.position.z;
            const float distXZ = std::sqrt(dx * dx + dz * dz);
            const float dy = std::abs(frame.playerPos.y - frame.position.y);
            if ( biteTimer_ > 0.0f ) {
                biteTimer_ -= frame.deltaTime;
                if ( biteTimer_ <= 0.0f ) { model.SetClip("Idle", true); }
            } else if ( frame.biteRange > 0.0f && distXZ < frame.biteRange && dy < kBiteHeightLimit ) {
                model.SetClip("Bite", false);
                model.SetAnimationTime(0.0f);
                biteTimer_ = kBiteTime;
            }
            // プレイヤーの方を向く（鉢は動かなくても、上体が向く）
            if ( distXZ < frame.biteRange + kFaceExtraRange && ( std::abs(dx) > kMinFaceDistance || std::abs(dz) > kMinFaceDistance ) ) {
                outYaw = std::atan2(dx, dz);
                return true;
            }
            return false;
        }

        void ReceiveHit(const EnemyHit& hit, Enemy& enemy, EnemyHitContext& context) const override{ hit.OnBiter(enemy, context); }

    private:
        float biteTimer_ = 0.0f; // 噛みつきの残り時間（0＝Idle 中）
    };

    // 空中「フワリン」：ずっと羽ばたく
    class FlyerBehavior : public EnemyBehavior {
    public:
        const char* StartClip(bool /*patrols*/) const override{ return "Fly"; }
        void OnReapplied(SkinnedObj3d& model) override{ model.SetClip("Fly", true); }
        bool Update(SkinnedObj3d& /*model*/, const Frame& /*frame*/, float& /*outYaw*/) override{ return false; }
        void ReceiveHit(const EnemyHit& hit, Enemy& enemy, EnemyHitContext& context) const override{ hit.OnFlyer(enemy, context); }
    };
}

std::unique_ptr<EnemyBehavior> EnemyBehavior::Create(EnemyType type){
    switch ( type ) {
    case EnemyType::Strong: return std::make_unique<BiterBehavior>();
    case EnemyType::Air:    return std::make_unique<FlyerBehavior>();
    default:                return std::make_unique<WalkerBehavior>();
    }
}
