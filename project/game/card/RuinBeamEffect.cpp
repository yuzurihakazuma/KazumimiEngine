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
        casterPos.x + forward.x * (baseScale_.z * 0.90f),
        casterPos.y - 0.8f,
        casterPos.z + forward.z * (baseScale_.z * 0.90f)
    };
    rot_ = { 0.0f, casterYaw, 0.0f };

    obj_ = Obj3d::Create("sphere");
    if (obj_) {
        obj_->SetCamera(camera);
        obj_->SetRotation(rot_);
        obj_->SetTranslation(pos_);
        obj_->SetScale(baseScale_);

        Model *model = obj_->GetModel();
        if (model) {
            // テクスチャを白紙に変更
            model->SetTexture("resources/white1x1.png");

            Model::Material *material = model->GetMaterial();
            if (material) {
                // ビームの色と透明度を設定 { R, G, B, A }
                // 禍々しい赤紫色（A=0.4f で少し透けるように設定）
                material->color = { 1.0f, 0.0f, 0.5f, 0.4f };

                // 発光（Emissive）を強くして、中央が白く光るようにする
                material->emissive = 3.0f;
            }
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
        float t = 1.0f - (static_cast<float>(lifeTimer_) / 72.0f);
        float pulse = 1.0f + std::sinf(t * 12.56636f) * 0.12f;

        // 出始めは細く、途中で一気に広がる
        float grow = std::clamp(t * 1.8f, 0.0f, 1.0f);
        float width = (0.35f + grow * 1.10f) * pulse;
        float height = 0.45f + grow * 0.75f;

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
