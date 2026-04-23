#include "BossSummonEffect.h"
#include "engine/math/VectorMath.h"
#include "game/enemy/Boss.h"
#include "game/enemy/EnemyManager.h"

void BossSummonEffect::Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera) {

    isFinished_ = false;
    timer_ = 60;

    // ボスの足元にエフェクトを出す
    pos_ = casterPos;
    pos_.y -= 2.0f; // 地面に埋まらないように少し浮かせる

    // 魔法陣の代わりに平べったい球体を出す
    obj_ = Obj3d::Create("sphere");
    if (obj_) {
        obj_->SetCamera(camera);
        obj_->SetTranslation(pos_);
        obj_->SetScale({ 4.0f, 0.1f, 4.0f }); // 平べったく広く
        Model *model = obj_->GetModel();
        if (model) {
            model->SetTexture("resources/white1x1.png");

            Model::Material *material = model->GetMaterial();
            if (material) {
                // 色と透明度の設定
                material->color = { 1.0f, 0.0f, 0.0f, 0.3f }; // 半透明の紫色
                material->emissive = 2.0f;                    // 発光の強さ
            }
        }
        obj_->Update();
    }
}

void BossSummonEffect::Update(Player *player, EnemyManager *enemyManager, Boss *boss,  const Vector3 &bossPos, const LevelData &level) {
    if (isFinished_) {
        return;
    }

    timer_--;

    if (obj_) {
        // 魔法陣を回転させる演出
        Vector3 rot = obj_->GetRotation();
        rot.y += 0.1f;
        obj_->SetRotation(rot);
        obj_->Update();
    }

    // ★演出終了時にボスへ召喚をリクエスト！
    if (timer_ <= 0) {
        if (boss && !boss->IsDead()) {
            boss->RequestSummon(spawnCount_); // Boss.hに追加した関数を呼ぶ
        }
        isFinished_ = true;
    }

    if (timer_ <= 0) {
        isFinished_ = true;
    }
}

void BossSummonEffect::Draw() {
    if (!isFinished_ && obj_) {
        obj_->Draw();
    }
}
