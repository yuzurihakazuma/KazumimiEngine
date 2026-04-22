#include "RuinBeamEffect.h"

#include "engine/math/VectorMath.h"
#include "game/enemy/Boss.h"
#include "game/player/Player.h"

#include <cmath>

using namespace VectorMath;

void RuinBeamEffect::Start(const Vector3& casterPos, float casterYaw, bool isPlayerCaster, Camera* camera) {
    (void)isPlayerCaster;

    isFinished_ = false;
    hitTimer_ = 0;
    lifeTimer_ = 72;

    Vector3 forward = {
        std::sinf(casterYaw),
        0.0f,
        std::cosf(casterYaw)
    };

    // 発射直後から前方を大きく塞ぐように、中心を少し前へ置く
    pos_ = {
        casterPos.x + forward.x * (baseScale_.z * 0.58f),
        casterPos.y - 0.8f,
        casterPos.z + forward.z * (baseScale_.z * 0.58f)
    };
    rot_ = { 0.0f, casterYaw, 0.0f };

    obj_ = Obj3d::Create("sphere");
    if (obj_) {
        obj_->SetCamera(camera);
        obj_->SetRotation(rot_);
        obj_->SetTranslation(pos_);
        obj_->SetScale(baseScale_);

        if (obj_->GetModel() && obj_->GetModel()->GetMaterial()) {
            auto* material = obj_->GetModel()->GetMaterial();
            material->color = { 1.0f, 0.25f, 0.25f, 0.92f };
            material->emissive = 3.4f;
        }

        obj_->Update();
    }
}

void RuinBeamEffect::Update(Player* player, EnemyManager* enemyManager, Boss* boss, const Vector3& bossPos, const LevelData& level) {
    (void)enemyManager;
    (void)bossPos;
    (void)level;

    if (isFinished_) {
        return;
    }

    if (hitTimer_ > 0) {
        hitTimer_--;
    }

    if (obj_) {
        // 出始めから中盤にかけて太くなり、終わり際に少し細くなる
        float t = 1.0f - (static_cast<float>(lifeTimer_) / 72.0f);
        float pulse = 1.0f + std::sinf(t * 6.28318f) * 0.10f;
        float width = (0.85f + std::sinf(t * 3.14159f) * 0.35f) * pulse;
        float height = 0.90f + std::sinf(t * 3.14159f) * 0.18f;

        obj_->SetScale({
            baseScale_.x * width,
            baseScale_.y * height,
            baseScale_.z
        });
        obj_->SetRotation(rot_);
        obj_->SetTranslation(pos_);
        obj_->Update();
    }

    if (player && !player->IsDead() && hitTimer_ <= 0) {
        if (IsPlayerInsideBeam(player->GetPosition())) {
            int finalDamage = damage_;
            if (boss && boss->IsAttackDebuffed()) {
                finalDamage /= 2;
            }

            player->TakeDamage(finalDamage, pos_);
            hitTimer_ = hitInterval_;
        }
    }

    lifeTimer_--;
    if (lifeTimer_ <= 0) {
        isFinished_ = true;
    }
}

void RuinBeamEffect::Draw() {
    if (!isFinished_ && obj_) {
        obj_->Draw();
    }
}

bool RuinBeamEffect::IsPlayerInsideBeam(const Vector3& playerPos) const {
    Vector3 diff = {
        playerPos.x - pos_.x,
        0.0f,
        playerPos.z - pos_.z
    };

    float sinY = std::sinf(-rot_.y);
    float cosY = std::cosf(-rot_.y);

    float localX = diff.x * cosY - diff.z * sinY;
    float localZ = diff.x * sinY + diff.z * cosY;

    const float halfWidth = baseScale_.x * 1.0f;
    const float halfLength = baseScale_.z * 1.05f;

    return std::fabs(localX) <= halfWidth && std::fabs(localZ) <= halfLength;
}
