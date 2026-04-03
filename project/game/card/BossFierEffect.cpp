#include "BossFierEffect.h"
#include "game/player/Player.h"
#include "engine/math/VectorMath.h"
#include "game/enemy/Boss.h"
#include <cmath>

using namespace VectorMath;

void BossFierEffect::Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera) {
    isFinished_ = false;

    // ボスが向いている方向（プレイヤーの方向）のベクトルを計算
    Vector3 forward = {
        std::sinf(casterYaw),
        0.0f,
        std::cosf(casterYaw)
    };

    // ボスの少し前から発射する
    pos_ = {
        casterPos.x + forward.x * 2.0f,
        casterPos.y - 2.0f, // 地面スレスレではなく少し浮かす
        casterPos.z + forward.z * 2.0f
    };

    // 弾のスピード（避けられるように少し遅めに設定。速くしたい場合は数字を大きく！）
    float speed = 0.5f;
    velocity_ = {
        forward.x * speed,
        0.0f,
        forward.z * speed
    };

    // オブジェクトの生成（"sphere" を赤いテクスチャなどに変えてもOKです！）
    obj_ = Obj3d::Create("sphere");
    if (obj_) {
        obj_->SetCamera(camera);
        obj_->SetScale(scale_);
        obj_->SetTranslation(pos_);
        obj_->Update();
    }
}

void BossFierEffect::Update(Player *player, EnemyManager *enemyManager, Boss *boss, const Vector3 &enemyPos, const Vector3 &bossPos, const LevelData &level) {

    if (isFinished_) {
        return;
    }

    // 弾を前へ進める
    pos_.x += velocity_.x;
    pos_.z += velocity_.z;

    // オブジェクトの位置を更新
    if (obj_) {
        // 炎が回転しながら飛んでいくとカッコいいので少し回す
        Vector3 rot = obj_->GetRotation();
        rot.x += 0.1f;
        rot.z += 0.1f;
        obj_->SetRotation(rot);

        obj_->SetTranslation(pos_);
        obj_->Update();
    }

    // --------------------------------------------------
    // プレイヤーへの当たり判定
    // --------------------------------------------------
    if (player && !player->IsDead()) {
        Vector3 playerPos = player->GetPosition();
        Vector3 diff = {
            playerPos.x - pos_.x,
            playerPos.y - pos_.y,
            playerPos.z - pos_.z
        };

        // スケールが大きいので当たり判定も広くする（2.0fくらい）
        if (Length(diff) < 2.0f) {
            int finalDamage = damage_;
            if (boss && boss->IsAttackDebuffed()) {
                finalDamage = finalDamage / 2;
            }
            // PlayerのTakeDamageは「ダメージ量」と「攻撃が飛んできた位置」を渡す
            player->TakeDamage(finalDamage, pos_);
            isFinished_ = true; // 当たったら消える
            return;
        }
    }

    // 寿命が尽きたら消える（壁抜け防止）
    lifeTimer_--;
    if (lifeTimer_ <= 0) {
        isFinished_ = true;
    }
}

void BossFierEffect::Draw() {
    if (!isFinished_ && obj_) {
        obj_->Draw();
    }
}


