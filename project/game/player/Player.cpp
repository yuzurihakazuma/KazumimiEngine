#include "game/player/Player.h"
#include "Engine/Base/Input.h"
#include "engine/math/VectorMath.h"
#include <cmath>

using namespace VectorMath;

void Player::Initialize() {
    pos_ = { 0.0f, 0.0f, 0.0f };
    rot_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 1.0f, 1.0f, 1.0f };

    isDodging_ = false;
    dodgeTimer_ = 0;
    dodgeDirection_ = { 0.0f, 0.0f, 0.0f };

    isActionLocked_ = false;  // 行動ロック初期化
    actionLockTimer_ = 0;

    level_ = 1;               // レベル初期化
    exp_ = 0;                 // 経験値初期化
    nextLevelExp_ = 3;        // 次レベル必要経験値初期化

    cost_ = 3;                // 現在コスト初期化
    maxCost_ = 3;             // 最大コスト初期化

    costRecoveryTimer_ = 0;   // コスト回復タイマー初期化
    costRecoveryInterval_ = 180; // コスト回復速度初期化

    hp_ = 5;                // 現在HP初期化
    maxHp_ = 5;             // 最大HP初期化
    isDead_ = false;        // 死亡状態リセット

    isInvincible_ = false;  // 無敵状態リセット
    invincibleTimer_ = 0;   // 無敵時間リセット

    isHit_ = false;           // 被弾状態リセット
    hitTimer_ = 0;            // 被弾時間リセット

    // ノックバック初期化
    isKnockback_ = false;                          // ノックバック状態リセット
    knockbackTimer_ = 0;                           // ノックバック時間リセット
    knockbackVelocity_ = { 0.0f, 0.0f, 0.0f };    // ノックバック速度リセット

    //速度
    speedMultiplier_ = 1.0f;
    speedBuffTimer_ = 0;

    //シールド
    isShieldActive_ = false;
   
}

void Player::Update() {

    if (isDead_) {
        return; // 死亡中は更新しない
    }

    Input* input = Input::GetInstance();

    Vector3 move{ 0.0f,0.0f,0.0f };

    UpdateCost(); // コスト自然回復

    // 被弾後無敵時間の更新
    if (isInvincible_) {
        invincibleTimer_--;

        if (invincibleTimer_ <= 0) {
            isInvincible_ = false;
            invincibleTimer_ = 0;
        }
    }


    // 被弾演出時間の更新
    if (isHit_) {
        hitTimer_--; // 被弾演出残り時間減少

        if (hitTimer_ <= 0) {
            isHit_ = false; // 被弾演出終了
            hitTimer_ = 0;
        }
    }

    // ノックバック中の更新
    if (isKnockback_) {
        pos_ += knockbackVelocity_;       // ノックバック移動
        knockbackVelocity_ *= 0.85f;      // 徐々に減速

        knockbackTimer_--;                // ノックバック残り時間減少

        if (knockbackTimer_ <= 0) {
            isKnockback_ = false;         // ノックバック終了
            knockbackTimer_ = 0;
            knockbackVelocity_ = { 0.0f, 0.0f, 0.0f };
        }
    }

    // 行動ロック中は操作不可
    if ( !isInputEnabled_ || isActionLocked_ ) {
        if ( isActionLocked_ ) {
            actionLockTimer_--;
            if ( actionLockTimer_ <= 0 ) {
                isActionLocked_ = false;
            }
        }
        return; // ここでリターンすればWASD入力は処理されない
    }

    // WASD入力
    if (input->Pushkey(DIK_W)) { move.z += 1.0f; }
    if (input->Pushkey(DIK_S)) { move.z -= 1.0f; }
    if (input->Pushkey(DIK_A)) { move.x -= 1.0f; }
    if (input->Pushkey(DIK_D)) { move.x += 1.0f; }

    // 移動方向を正規化
    if (Length(move) > 0.0f) {
        move = Normalize(move);
    }

    // 回避開始
    if (!isDodging_ && input->Triggerkey(DIK_LSHIFT)) {

        isDodging_ = true;             // 回避状態
        dodgeTimer_ = dodgeDuration_;  // 回避時間

        if (Length(move) > 0.0f) {
            dodgeDirection_ = move;                 // 入力方向へ回避
            rot_.y = std::atan2f(move.x, move.z);   // 回避方向へ向く
        } else {
            dodgeDirection_ = {
                std::sinf(rot_.y),
                0.0f,
                std::cosf(rot_.y)
            }; // 向いている方向へ回避
        }
    }

    if (isDodging_) {

        pos_ += dodgeDirection_ * dodgeSpeed_; // 回避移動

        rot_.x += 0.5f; // 回避中の回転演出

        dodgeTimer_--; // 回避時間減少

        if (dodgeTimer_ <= 0) {
            isDodging_ = false; // 回避終了
            rot_.x = 0.0f;      // 回転リセット
        }

    } else {

        if (Length(move) > 0.0f) {
            pos_ += move * (moveSpeed_*speedMultiplier_);            // 通常移動
            rot_.y = std::atan2f(move.x, move.z); // 移動方向へ向く
        }
    }

    // スピードバフの更新
    if (speedBuffTimer_ > 0) {
        speedBuffTimer_--;
        // タイマーが0になったら通常の速度に戻す
        if (speedBuffTimer_ <= 0) {
            speedMultiplier_ = 1.0f;
        }
    }

    
}

// 指定フレームの間プレイヤー操作をロック
void Player::LockAction(int frame) {
    isActionLocked_ = true;
    actionLockTimer_ = frame;
}

void Player::AddExp(int value) {

    exp_ += value; // 経験値加算

    while (exp_ >= nextLevelExp_) {
        exp_ -= nextLevelExp_; // 必要経験値分を消費
        LevelUp();             // レベルアップ
    }
}

bool Player::CanUseCost(int value) const {
    return cost_ >= value; // 必要コスト以上あるか
}

void Player::UseCost(int value) {

    if (cost_ < value) return; // 足りなければ消費しない

    cost_ -= value; // コスト消費

    if (cost_ < 0) {
        cost_ = 0; // 下限補正
    }
}

void Player::Heal(int amount) {
    if (isDead_) {
        return; // 死亡中は回復しない
    }

    // HP回復
    hp_ += amount;

    // 最大HPを超えないように修正
    if (hp_ > maxHp_) {
        hp_ = maxHp_;
    }
}


void Player::LevelUp() {

    level_++;            // レベル上昇
    nextLevelExp_ += 2;  // 次に必要な経験値を増やす

    maxCost_ += 1;       // 最大コスト増加
    cost_ = maxCost_;    // レベルアップ時に全回復

    if (costRecoveryInterval_ > 60) {
        costRecoveryInterval_ -= 15; // 回復速度アップ
    }
}

void Player::UpdateCost() {

    if (cost_ >= maxCost_) return; // 最大まで回復していたら何もしない

    costRecoveryTimer_++; // タイマー進行

    if (costRecoveryTimer_ >= costRecoveryInterval_) {
        costRecoveryTimer_ = 0; // タイマーリセット
        cost_ += 1;             // コスト回復

        if (cost_ > maxCost_) {
            cost_ = maxCost_;   // 上限補正
        }
    }
}

// ダメージ処理
void Player::TakeDamage(int damage, const Vector3& attackFrom) {

    // 既に死亡していたら無視
    if (isDead_) {
        return;
    }

    // 回避中はダメージ無効
    if (isDodging_) {
        return;
    }

    // 無敵中はダメージ無効
    if (isInvincible_) {
        return;
    }

    // もしシールドが残っていたら、回数を減らしてダメージを無効化！
    if (shieldHitCount_ > 0) {
        shieldHitCount_--;
        return; // ここで処理を抜けるのでダメージを受けない
    }

    // HP減少
    hp_ -= damage;
    if (hp_ < 0) {
        hp_ = 0;
    }

    // 被弾演出開始
    isHit_ = true;
    hitTimer_ = hitDuration_;

    // 被弾後無敵開始
    isInvincible_ = true;
    invincibleTimer_ = invincibleDuration_;

    // 攻撃元からプレイヤーへの方向を計算
    Vector3 hitDir = {
        pos_.x - attackFrom.x,
        0.0f,
        pos_.z - attackFrom.z
    };

    // ノックバック開始
    if (Length(hitDir) > 0.01f) {
        hitDir = Normalize(hitDir);
        knockbackVelocity_ = hitDir * 0.35f;
        isKnockback_ = true;
        knockbackTimer_ = knockbackDuration_;
    }

    // HPが0なら死亡
    if (hp_ <= 0) {
        isDead_ = true;
    }
}

void Player::ApplySpeedBuff(float multiplier, int durationFrames) {
    speedMultiplier_ = multiplier;
    speedBuffTimer_ = durationFrames;
}

// 描画するか判定
bool Player::IsVisible() const {

    // 被弾中でなければ常に表示
    if (!isHit_) {
        return true;
    }

    // 被弾中は1フレームおきに表示して点滅させる
    return (hitTimer_ % 2) == 0;
}