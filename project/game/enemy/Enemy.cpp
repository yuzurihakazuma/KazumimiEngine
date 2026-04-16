#include "game/enemy/Enemy.h"
#include "engine/math/VectorMath.h"
#include "game/card/CardDatabase.h"
#include <cmath>

using namespace VectorMath;

void Enemy::Initialize() {
    pos_ = { 5.0f, 0.0f, 5.0f };
    rot_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 1.0f, 1.0f, 1.0f };

    startX_ = pos_.x;
    state_ = State::Patrol;
    baseCard_ = CardDatabase::GetCardData(1); // 固定のパンチカード
    hasPickupCard_ = false;
    pickupCard_ = { -1, "", 0 };
    currentUseCard_ = { -1, "", 0 };
    pickupCardTimer_ = 0;                     // 拾ったカードタイマー初期化

    hp_ = 3;                 // 敵HP初期化
    isDead_ = false;         // 死亡状態リセット
    isActionLocked_ = false; // 行動ロック解除
    actionLockTimer_ = 0;    // ロック時間リセット
    thinkTimer_ = 0;         // 思考タイマー初期化

    isHit_ = false;                            // ヒット状態リセット
    hitTimer_ = 0;                             // ヒット時間リセット
    hitStunTimer_ = 0;                         // 被弾硬直時間リセット
    knockbackVelocity_ = { 0.0f, 0.0f, 0.0f }; // ノックバック初期化

    cardUseRequest_ = false;  // カード使用発生フラグ初期化

    isCasting_ = false;
    castTimer_ = 0;
    strafeDirection_ = 1;
    cardCooldownTimer_ = 0;   // カード使用クールダウン初期化

    hasTargetCard_ = false;   // 目標カード状態リセット
    prevPos_ = pos_;          // 前フレーム位置初期化
    stuckTimer_ = 0;          // 詰まりタイマー初期化

    isFrozen_ = false;        // 凍結状態リセット
    freezeTimer_ = 0;         // 凍結時間リセット
}

void Enemy::Update() {

    if (isDead_) return; // 死亡していたら何もしない

    // もし「カード使用状態」じゃないのに詠唱フラグが残っていたら強制解除！
    if (state_ != State::UseCard && isCasting_) {
        isCasting_ = false;
        castTimer_ = 0;
    }

    cardUseRequest_ = false;  // 毎フレームカード使用フラグ初期化

    // 拾ったカードの使用可能時間を減らす
    if (hasPickupCard_) {
        pickupCardTimer_--;

        // 時間切れになったら拾ったカードを消す
        if (pickupCardTimer_ <= 0) {
            ClearPickupCard();
        }
    }

    if (isActionLocked_) {
        actionLockTimer_--;          // 残り時間を減らす
        if (actionLockTimer_ <= 0) {
            isActionLocked_ = false; // 0になったらロック解除
        }
        return;                      // ロック中は他の処理しない
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

    case State::UseCard:
        UpdateUseCard();
        break;

    case State::Retreat:
        UpdateRetreat();
        break;
    }

    // デバフタイマーの更新
    if (isAttackDebuffed_) {
        attackDebuffTimer_--;
        if (attackDebuffTimer_ <= 0) {
            isAttackDebuffed_ = false;
        }
    }
}

void Enemy::Freeze(int durationFrames) {
    isFrozen_ = true;
    freezeTimer_ = durationFrames;
}

// 次の状態を決める
void Enemy::DecideNextState() {

    if (isCasting_) {
        stuckTimer_ = 0; // 詰まりタイマーを常にリセット
        return;          // ここで処理を終わらせて、元の状態をキープ！
    }

    Vector3 toPlayer = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    float playerDist = Length(toPlayer); // プレイヤーとの距離
    float useRange = GetUseRangeForCurrentCard(); // 今使うカードの射程
    float exitRange = useRange + 1.5f; // 射程から少し離れたら追跡に戻す
    float retreatEnter = GetRetreatEnterRangeForCurrentCard(); // 今使うカードの離脱開始距離

    // 詰まっていたら一旦巡回に戻して方向転換
    if (IsStuck()) {
        state_ = State::Patrol;
        direction_ *= -1;
        strafeDirection_ *= -1;
        thinkTimer_ = 20;
        stuckTimer_ = 0;
        return;
    }

    // 拾ったカードを持っている場合
    if (hasPickupCard_) {

        if (state_ == State::UseCard) {
            if (playerDist < retreatEnter) {
                state_ = State::Retreat;
            } else if (playerDist > exitRange) {
                state_ = State::ChasePlayer;
            }
            return;
        }

        if (state_ == State::Retreat) {
            if (playerDist >= retreatExitRange_) {
                state_ = State::UseCard;
            }
            return;
        }

        if (playerDist < retreatEnter) {
            state_ = State::Retreat;
        } else if (playerDist <= useRange) {
            state_ = State::UseCard;
        } else if (playerDist <= chaseRange_) {
            state_ = State::ChasePlayer;
        } else {
            state_ = State::Patrol;
        }

        return;
    }

    // 拾ったカードを持っていない場合
    if (hasTargetCard_) {
        state_ = State::MoveToCard;
    } else if (playerDist <= useRange) {
        state_ = State::UseCard;
    } else if (playerDist <= chaseRange_) {
        state_ = State::ChasePlayer;
    } else {
        state_ = State::Patrol;
    }
}

// 詰まり判定
bool Enemy::IsStuck() const {
    return stuckTimer_ >= stuckThreshold_; // 一定時間動いていなければ詰まり
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

void Enemy::UpdateUseCard() {

    Vector3 dir = {
        playerPos_.x - pos_.x,
        0.0f,
        playerPos_.z - pos_.z
    };

    float dist = Length(dir); // プレイヤーとの距離
    float useRange = GetUseRangeForCurrentCard(); // 今使うカードの射程

    // ★ 修正1：クールダウン（前回の攻撃の疲れ）が残っているなら、
    // そもそも詠唱を始めず、プレイヤーの追跡に戻る！
    // =========================================================
    if (cardCooldownTimer_ > 0) {
        state_ = State::ChasePlayer;
        thinkTimer_ = 0;
        isCasting_ = false;
        castTimer_ = 0;
        return;
    }

    // =========================================================
    // ★ 修正2：「詠唱が始まる前」にプレイヤーが射程外にいたら追跡に戻る。
    // （一度詠唱を始めたら、途中で逃げられてもキャンセルしない！）
    // =========================================================
    if (!isCasting_ && dist > useRange + 1.5f) {
        state_ = State::ChasePlayer;
        thinkTimer_ = 0;
        return;
    }

    // 射程外なら撃たずに追跡へ戻す
    if (dist > useRange + 1.5f) {
        state_ = State::ChasePlayer;
        thinkTimer_ = 0;
        isCasting_ = false;
        castTimer_ = 0;
        return;
    }

    // プレイヤー方向を向く
    if (dist > 0.01f) {
        rot_.y = std::atan2f(dir.x, dir.z);
    }

    // 詠唱開始
    if (!isCasting_) {
        isCasting_ = true;
        castTimer_ = castTime_;

        // 詠唱が始まった瞬間に「今回使うカード」をセットしておく！
        if (hasPickupCard_) {
            currentUseCard_ = pickupCard_;
        } else {
            currentUseCard_ = baseCard_;
        }

        return;
    }

    //// 詠唱中に射程外になったら中断
    //if (dist > useRange) {
    //    state_ = State::ChasePlayer;
    //    thinkTimer_ = 0;
    //    isCasting_ = false;
    //    castTimer_ = 0;
    //    return;
    //}

    // 詠唱中
    if (castTimer_ > 0) {
        castTimer_--;
        return;
    }

   

    bool usedPickupCard = hasPickupCard_;

    

    // カード使用
    cardUseRequest_ = true;
    cardCooldownTimer_ = cardCooldown_;

    isCasting_ = false;

    // 拾ったカードは使ってもすぐ消さない
    // 時間切れになるまで保持する

    // 発動後の硬直
    SetActionLock(8);

    // 使用後の行動
    if (usedPickupCard) {
        state_ = State::Retreat;
    } else {
        state_ = State::ChasePlayer;
    }

    thinkTimer_ = 5;
}
void Enemy::UpdateRetreat() {

    Vector3 dir = {
        pos_.x - playerPos_.x,
        0.0f,
        pos_.z - playerPos_.z
    }; // プレイヤーから離れる方向

    float dist = Length(dir); // プレイヤーとの距離
    float useRange = GetUseRangeForCurrentCard(); // 今使うカードの射程
    float retreatEnter = GetRetreatEnterRangeForCurrentCard(); // 今使うカードの離脱開始距離

    // 十分離れたらカード使用へ戻す
    if (dist >= retreatEnter + 0.5f) {
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
    isCasting_ = false;
    castTimer_ = 0;
    currentUseCard_ = { -1, "", 0 };
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

float Enemy::GetUseRangeForCurrentCard() const {
    const Card& card = hasPickupCard_ ? pickupCard_ : baseCard_;

    // カードの攻撃距離タイプで使用距離を変える
    switch (card.attackRangeType) {
    case CardAttackRangeType::Melee:
        return 1.8f; // 近距離

    case CardAttackRangeType::Mid:
        return 4.0f; // 中距離

    case CardAttackRangeType::Long:
        return 7.0f; // 遠距離
    }

    // 念のため
    return cardUseEnterRange_;
}

float Enemy::GetRetreatEnterRangeForCurrentCard() const {
    const Card& card = hasPickupCard_ ? pickupCard_ : baseCard_;

    // カードの攻撃距離タイプで離脱開始距離を変える
    switch (card.attackRangeType) {
    case CardAttackRangeType::Melee:
        return 1.0f; // 近距離はあまり逃げない

    case CardAttackRangeType::Mid:
        return 2.5f; // 中距離は少し距離を取る

    case CardAttackRangeType::Long:
        return 4.0f; // 遠距離は近づかれたら逃げる
    }

    // 念のため
    return retreatEnterRange_;
}