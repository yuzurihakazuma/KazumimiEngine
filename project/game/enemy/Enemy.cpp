#include "game/enemy/Enemy.h"
#include "engine/math/VectorMath.h"
#include <cmath>

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

    // 攻撃・カード使用リクエスト初期化
    attackRequest_ = false; // 近接攻撃発生フラグ初期化
    cardUseRequest_ = false; // カード使用発生フラグ初期化

    // クールダウン初期化
    attackCooldownTimer_ = 0; // 近接攻撃クールダウン初期化
    cardCooldownTimer_ = 0;   // カード使用クールダウン初期化

    hasTargetCard_ = false;                 // 目標カード状態リセット
    prevPos_ = pos_;                        // 前フレーム位置初期化
    stuckTimer_ = 0;                        // 詰まりタイマー初期化
}

void Enemy::Update() {

    if (isDead_) return; // 死亡していたら何もしない

    // このフレームの攻撃・カード使用フラグをリセット
    attackRequest_ = false;   // 毎フレーム初期化
    cardUseRequest_ = false;  // 毎フレーム初期化

    // 近接攻撃クールダウン更新
    if (attackCooldownTimer_ > 0) {
        attackCooldownTimer_--; // クールダウン減少
    }

    // カード使用クールダウン更新
    if (cardCooldownTimer_ > 0) {
        cardCooldownTimer_--; // クールダウン減少
    }

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

    // 詰まり判定更新
    if (Length(pos_ - prevPos_) < stuckDistanceThreshold_) {
        stuckTimer_++; // ほとんど動いていなければ加算
    } else {
        stuckTimer_ = 0; // 動いていればリセット
    }
    prevPos_ = pos_; // 現在位置を保存

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

    // 思考タイマーが切れていたら次の状態を決定
    if (thinkTimer_ <= 0) {
        DecideNextState();
    }

    //　凍結中の処理
    if (isFrozen_) {
        freezeTimer_--; // 凍結残り時間減少
        if (freezeTimer_ <= 0) {
            isFrozen_ = false; // 時間切れで氷が溶ける
        }
        return;
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

    case State::Retreat:
        UpdateRetreat();
        break;
    }
}

void Enemy::Freeze(int durationFrames) {
    isFrozen_ = true;
    freezeTimer_ = durationFrames;
}

// 次の状態を決める
void Enemy::DecideNextState() {

    Vector3 toPlayer = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    float playerDist = Length(toPlayer); // プレイヤーとの距離

    // 詰まっていたら一旦巡回に戻して方向転換
    if (IsStuck()) {
        state_ = State::Patrol; // 巡回へ戻す
        direction_ *= -1;       // 巡回方向反転
        thinkTimer_ = 20;       // 少しの間判断固定
        stuckTimer_ = 0;        // 詰まり状態解除
        return;
    }

    // カードを持っている場合
    if (hasCard_) {

        // 近接カードを持っている場合
        if (HasMeleeCard()) {

            // すでにカード使用中なら、少し離れても維持する
            if (state_ == State::UseCard) {
                if (playerDist > attackExitRange_) {
                    state_ = State::ChasePlayer; // 離れすぎたら追跡へ戻る
                }
            } else {
                if (playerDist <= attackEnterRange_) {
                    state_ = State::UseCard;     // 近ければカード使用
                } else if (playerDist <= chaseRange_) {
                    state_ = State::ChasePlayer; // 射程外なら追跡
                } else {
                    state_ = State::Patrol;      // 遠ければ巡回
                }
            }

            return;
        }

        // 遠距離カードを持っている場合
        if (HasRangedCard()) {

            // 離れる状態中なら、十分離れるまで維持
            if (state_ == State::Retreat) {
                if (playerDist >= retreatExitRange_) {
                    state_ = State::UseCard; // 十分離れたらカード使用へ
                }
                return;
            }

            // カード使用中なら、少し離れても維持
            if (state_ == State::UseCard) {
                if (playerDist < retreatEnterRange_) {
                    state_ = State::Retreat;     // 近すぎたら離れる
                } else if (playerDist > cardUseExitRange_) {
                    state_ = State::ChasePlayer; // 離れすぎたら追跡
                }
                return;
            }

            // 通常時の判定
            if (playerDist < retreatEnterRange_) {
                state_ = State::Retreat;         // 近すぎる
            } else if (playerDist <= cardUseEnterRange_) {
                state_ = State::UseCard;         // 射程内
            } else if (playerDist <= chaseRange_) {
                state_ = State::ChasePlayer;     // 追跡
            } else {
                state_ = State::Patrol;          // 遠い
            }

            return;
        }

        // その他カード
        if (state_ == State::UseCard) {
            if (playerDist > cardUseExitRange_) {
                state_ = State::ChasePlayer;
            }
        } else {
            if (playerDist <= cardUseEnterRange_) {
                state_ = State::UseCard;
            } else if (playerDist <= chaseRange_) {
                state_ = State::ChasePlayer;
            } else {
                state_ = State::Patrol;
            }
        }

        return;
    }

    // カードを持っていない場合
    if (state_ == State::AttackPlayer) {
        if (playerDist > attackExitRange_) {
            state_ = State::ChasePlayer; // 離れたら追跡へ戻る
        }
        return;
    }

    if (playerDist <= attackEnterRange_) {
        state_ = State::AttackPlayer; // 近ければ通常攻撃
    } else if (hasTargetCard_) {
        state_ = State::MoveToCard;   // カードが見つかっていれば拾いに行く
    } else if (playerDist <= chaseRange_) {
        state_ = State::ChasePlayer;  // プレイヤー追跡
    } else {
        state_ = State::Patrol;       // 何もなければ巡回
    }
}

// 詰まり判定
bool Enemy::IsStuck() const {
    return stuckTimer_ >= stuckThreshold_; // 一定時間動いていなければ詰まり
}

// 近接カードを持っているか
bool Enemy::HasMeleeCard() const {
    return hasCard_ && heldCard_.id == 1; // 例：Fist
}

// 遠距離カードを持っているか
bool Enemy::HasRangedCard() const {
    return hasCard_ && heldCard_.id == 2; // 例：Fireball
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
        pos_ += dir * chaseSpeed_;               // カードへ移動
        rot_.y = std::atan2f(dir.x, dir.z);     // 移動方向を向く
    }
}

void Enemy::UpdateChasePlayer() {

    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    if (Length(dir) > 0.01f) {
        dir = Normalize(dir);
        pos_ += dir * chaseSpeed_;               // プレイヤー追跡
        rot_.y = std::atan2f(dir.x, dir.z);     // プレイヤー方向を向く
    }
}

void Enemy::UpdateAttackPlayer() {

    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    }; // プレイヤー方向

    float dist = Length(dir); // プレイヤーとの距離

    // 十分離れたら追跡に戻す
    if (dist > attackExitRange_) {
        state_ = State::ChasePlayer; // 追跡へ戻す
        thinkTimer_ = 0;             // すぐ再判断できるようにする
        return;
    }

    // プレイヤー方向を向く
    if (dist > 0.01f) {
        rot_.y = std::atan2f(dir.x, dir.z);
    }

    // クールダウン中は攻撃しない
    if (attackCooldownTimer_ > 0) {
        return;
    }

    // 近接攻撃発生
    attackRequest_ = true;
    attackCooldownTimer_ = attackCooldown_;

    SetActionLock(10);
    thinkTimer_ = 10;
}

void Enemy::UpdateUseCard() {

    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    }; // プレイヤー方向

    float dist = Length(dir); // プレイヤーとの距離

    // 十分離れたら追跡へ戻す
    if (dist > cardUseExitRange_) {
        state_ = State::ChasePlayer;
        thinkTimer_ = 0;
        return;
    }

    // プレイヤー方向を向く
    if (dist > 0.01f) {
        rot_.y = std::atan2f(dir.x, dir.z);
    }

    // クールダウン中はカードを使わない
    if (cardCooldownTimer_ > 0) {
        return;
    }

    // カード使用発生
    cardUseRequest_ = true;
    cardCooldownTimer_ = cardCooldown_;

    SetActionLock(15);
    thinkTimer_ = 15;
}

void Enemy::UpdateRetreat() {

    Vector3 dir = {
        pos_.x - playerPos_.x,
        0.0f,
        pos_.z - playerPos_.z
    }; // プレイヤーから離れる方向

    float dist = Length(dir); // プレイヤーとの距離

    // 十分離れたらカード使用へ戻す
    if (dist >= retreatExitRange_) {
        state_ = State::UseCard;
        thinkTimer_ = 0;
        return;
    }

    if (dist > 0.01f) {
        dir = Normalize(dir);
        pos_ += dir * chaseSpeed_;
        rot_.y = std::atan2f(dir.x, dir.z);
    }
}
void Enemy::TakeDamage(int damage) {

    if (isDead_) return; // 既に死亡していたら無視

    // 被弾でカード詠唱キャンセル
    if (state_ == State::UseCard) {
        cardUseRequest_ = false;
        isActionLocked_ = false;
        actionLockTimer_ = 0;
        state_ = State::ChasePlayer;
        thinkTimer_ = 10; // 少しだけ立て直し時間
    }

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