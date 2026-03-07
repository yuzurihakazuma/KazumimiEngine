#include "game/enemy/Enemy.h"
#include "engine/math/VectorMath.h"

using namespace VectorMath;

void Enemy::Initialize() {
    pos_ = { 5.0f, 0.0f, 5.0f };
    rot_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 1.0f, 1.0f, 1.0f };

    startX_ = pos_.x;
    state_ = State::Patrol;
    hasCard_ = false;

    hp_ = 3;                 // 敵HP初期化
    isDead_ = false;         // 死亡状態リセット
    isActionLocked_ = false; // 行動ロック解除
    actionLockTimer_ = 0;    // ロック時間リセット
    thinkTimer_ = 0;         // 思考タイマー初期化

    isHit_ = false;                            // ヒット状態リセット
    hitTimer_ = 0;                             // ヒット時間リセット
    knockbackVelocity_ = { 0.0f, 0.0f, 0.0f }; // ノックバック初期化
}

void Enemy::Update() {

    if (isDead_) return; // 死亡していたら何もしない

    // ヒット演出中
    if (isHit_) {
        pos_ += knockbackVelocity_;      // ノックバック移動
        knockbackVelocity_ *= 0.85f;     // 徐々に減速

        hitTimer_--;                     // 残り時間減少
        if (hitTimer_ <= 0) {
            isHit_ = false;              // ヒット演出終了
            knockbackVelocity_ = { 0.0f, 0.0f, 0.0f };
        }
    }

    // 行動ロック中はタイマーだけ進める
    if (isActionLocked_) {
        actionLockTimer_--; // ロック残り時間減少

        if (actionLockTimer_ <= 0) {
            isActionLocked_ = false; // ロック解除
        }

        return; // ロック中は行動しない
    }

    // 思考タイマー更新
    if (thinkTimer_ > 0) {
        thinkTimer_--; // 次の判断まで待つ
    }

    switch (state_) {
    case State::Patrol:
        UpdatePatrol();
        break;

    case State::MoveToCard:
        UpdateMoveToCard();
        break;

    case State::ChasePlayer:
        UpdateChasePlayer();
        break;

    case State::AttackPlayer:
        UpdateAttackPlayer();
        break;

    case State::UseCard:
        UpdateUseCard();
        break;
    }
}

void Enemy::UpdatePatrol() {

    pos_.x += speed_ * direction_; // 巡回移動

    if (direction_ > 0) { rot_.y = 1.57f; } // 右向き
    else { rot_.y = -1.57f; }               // 左向き

    if (pos_.x > startX_ + moveRange_) direction_ = -1; // 端で反転
    if (pos_.x < startX_ - moveRange_) direction_ = 1;
}

void Enemy::UpdateMoveToCard() {

    Vector3 dir = {
        targetCardPos_.x - pos_.x,
        0.0f,
        targetCardPos_.z - pos_.z
    };

    if (Length(dir) > 0.01f) {
        dir = Normalize(dir);
        pos_ += dir * chaseSpeed_; // カードへ移動
    }

    if (dir.x > 0.0f) { rot_.y = 1.57f; }     // 右向き
    else if (dir.x < 0.0f) { rot_.y = -1.57f; } // 左向き
}

void Enemy::UpdateChasePlayer() {

    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    if (Length(dir) > 0.01f) {
        dir = Normalize(dir);
        pos_ += dir * chaseSpeed_; // プレイヤー追跡
    }

    if (dir.x > 0.0f) { rot_.y = 1.57f; }     // 右向き
    else if (dir.x < 0.0f) { rot_.y = -1.57f; } // 左向き
}

void Enemy::UpdateAttackPlayer() {

    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    }; // プレイヤー方向

    if (dir.x > 0.0f) { rot_.y = 1.57f; }     // 右向き
    else if (dir.x < 0.0f) { rot_.y = -1.57f; } // 左向き

    SetActionLock(20); // 攻撃硬直
    thinkTimer_ = 20;  // 少しの間判断を固定
}

void Enemy::UpdateUseCard() {

    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    }; // プレイヤー方向

    if (dir.x > 0.0f) { rot_.y = 1.57f; }     // 右向き
    else if (dir.x < 0.0f) { rot_.y = -1.57f; } // 左向き

    SetActionLock(30); // カード使用硬直
    thinkTimer_ = 30;  // 少しの間判断を固定
}

void Enemy::TakeDamage(int damage) {

    if (isDead_) return; // 既に死亡していたら無視

    hp_ -= damage; // HP減少

    isHit_ = true;               // ヒット状態開始
    hitTimer_ = hitDuration_;    // ヒット時間セット

    Vector3 hitDir = {
        pos_.x - playerPos_.x,
        0.0f,
        pos_.z - playerPos_.z
    }; // プレイヤーから敵への方向

    if (Length(hitDir) > 0.01f) {
        hitDir = Normalize(hitDir);
        knockbackVelocity_ = hitDir * 0.25f; // 後ろへ少し飛ばす
    }

    if (hp_ <= 0) {
        hp_ = 0;         // HP下限
        isDead_ = true;  // 死亡
    }
}

void Enemy::SetActionLock(int frame) {
    isActionLocked_ = true; // 行動ロック開始
    actionLockTimer_ = frame; // ロック時間設定
}

bool Enemy::IsVisible() const {

    if (!isHit_) return true; // 通常時は常に表示

    return (hitTimer_ % 2) == 0; // ヒット中は1フレームおきに表示
}