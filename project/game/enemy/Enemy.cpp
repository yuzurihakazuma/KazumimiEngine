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
    hitStunTimer_ = 0;                         // 被弾硬直時間リセット
    knockbackVelocity_ = { 0.0f, 0.0f, 0.0f }; // ノックバック初期化

    attackRequest_ = false;   // 近接攻撃発生フラグ初期化
    cardUseRequest_ = false;  // カード使用発生フラグ初期化

    attackCooldownTimer_ = 0; // 近接攻撃クールダウン初期化
    cardCooldownTimer_ = 0;   // カード使用クールダウン初期化

    hasTargetCard_ = false;   // 目標カード状態リセット
    prevPos_ = pos_;          // 前フレーム位置初期化
    stuckTimer_ = 0;          // 詰まりタイマー初期化

    isFrozen_ = false;        // 凍結状態リセット
    freezeTimer_ = 0;         // 凍結時間リセット
}

void Enemy::Update() {

    if (isDead_) return; // 死亡していたら何もしない

    attackRequest_ = false;   // 毎フレーム攻撃フラグ初期化
    cardUseRequest_ = false;  // 毎フレームカード使用フラグ初期化

    if (isActionLocked_) {
        actionLockTimer_--;          // 残り時間を減らす
        if (actionLockTimer_ <= 0) {
            isActionLocked_ = false; // 0になったらロック解除（動けるようになる）
        }

        // 🌟超重要🌟
        // ロック中はこの下の「移動」や「攻撃」の処理を一切やらずにここで終わる！
        return;
    }

    if (attackCooldownTimer_ > 0) {
        attackCooldownTimer_--; // 近接攻撃クールダウン減少
    }

    if (cardCooldownTimer_ > 0) {
        cardCooldownTimer_--; // カード使用クールダウン減少
    }

    if (isHit_) {
        pos_ += knockbackVelocity_;      // ノックバック移動
        knockbackVelocity_ *= 0.85f;     // 徐々に減速

        hitTimer_--;                     // ヒット演出時間減少
        if (hitTimer_ <= 0) {
            isHit_ = false;              // ヒット演出終了
        }
    }

    if (hitStunTimer_ > 0) {
        hitStunTimer_--;                 // 被弾硬直時間減少

        if (hitStunTimer_ <= 0) {
            knockbackVelocity_ = { 0.0f, 0.0f, 0.0f }; // 硬直終了時に速度停止
        }

        prevPos_ = pos_;                 // 現在位置を保存
        return;                          // 硬直中はAI行動しない
    }

    if (Length(pos_ - prevPos_) < stuckDistanceThreshold_) {
        stuckTimer_++;                   // ほとんど動いていなければ加算
    } else {
        stuckTimer_ = 0;                 // 動いていればリセット
    }
    prevPos_ = pos_;                     // 現在位置を保存

    if (isActionLocked_) {
        actionLockTimer_--;              // ロック残り時間減少

        if (actionLockTimer_ <= 0) {
            isActionLocked_ = false;     // ロック解除
        }

        return;                          // ロック中は行動しない
    }

    if (thinkTimer_ > 0) {
        thinkTimer_--;                   // 次の判断まで待つ
    }

    if (thinkTimer_ <= 0) {
        DecideNextState();               // 次の状態を決定
    }

    if (isFrozen_) {
        freezeTimer_--;                  // 凍結残り時間減少
        if (freezeTimer_ <= 0) {
            isFrozen_ = false;           // 時間切れで氷が溶ける
        }
        return;                          // 凍結中は行動しない
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

    // デバフタイマーの更新
    if (isAttackDebuffed_) {
        attackCooldownTimer_--;
        if (attackCooldownTimer_ <= 0) {
            isAttackDebuffed_ = false; // 時間切れで元に戻る
        }
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

    if (state_ == State::UseCard) {
        cardUseRequest_ = false;         // カード使用要求をキャンセル
        isActionLocked_ = false;         // 行動ロック解除
        actionLockTimer_ = 0;            // ロック時間リセット
        state_ = State::ChasePlayer;     // 追跡状態へ戻す
    }

    hp_ -= damage;                       // HP減少

    isHit_ = true;                       // ヒット状態開始
    hitTimer_ = hitDuration_;            // ヒット演出時間セット
    hitStunTimer_ = hitStunDuration_;    // 被弾硬直開始

    thinkTimer_ = hitStunDuration_;      // 硬直中は再判断しない
    attackCooldownTimer_ = 20;           // 被弾後すぐ攻撃しないようにする
    cardCooldownTimer_ = 20;             // 被弾後すぐカードを使わないようにする

    Vector3 hitDir = {
        pos_.x - playerPos_.x,
        0.0f,
        pos_.z - playerPos_.z
    }; // プレイヤーから敵への方向

    if (Length(hitDir) > 0.01f) {
        hitDir = Normalize(hitDir);
        knockbackVelocity_ = hitDir * 0.18f; // 後ろへ少し飛ばす
    } else {
        knockbackVelocity_ = { 0.0f, 0.0f, 0.0f }; // 方向が取れないときは停止
    }

    if (hp_ <= 0) {
        hp_ = 0;                         // HP下限
        isDead_ = true;                  // 死亡
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