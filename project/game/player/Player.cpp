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
}

void Player::Update() {

    Input* input = Input::GetInstance();

    Vector3 move{ 0.0f,0.0f,0.0f };

    UpdateCost(); // コスト自然回復

    // 行動ロック中は操作不可
    if (isActionLocked_) {

        actionLockTimer_--; // ロック時間減少

        if (actionLockTimer_ <= 0) {
            isActionLocked_ = false; // ロック解除
        }

        return; // このフレームは操作処理をしない
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
            pos_ += move * moveSpeed_;            // 通常移動
            rot_.y = std::atan2f(move.x, move.z); // 移動方向へ向く
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