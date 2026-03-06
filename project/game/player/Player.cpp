#include "game/player/Player.h"
#include "Engine/Base/Input.h"
#include "engine/math/VectorMath.h"

using namespace VectorMath;

void Player::Initialize() {
    pos_ = { 0.0f, 0.0f, 0.0f };
    rot_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 1.0f, 1.0f, 1.0f };

    isDodging_ = false;
    dodgeTimer_ = 0;
    dodgeDirection_ = { 0.0f, 0.0f, 0.0f };
}

void Player::Update() {
    Input* input = Input::GetInstance();

    Vector3 move{ 0.0f, 0.0f, 0.0f };

    if (input->Pushkey(DIK_W)) { move.z += 1.0f; }
    if (input->Pushkey(DIK_S)) { move.z -= 1.0f; }
    if (input->Pushkey(DIK_A)) { move.x -= 1.0f; }
    if (input->Pushkey(DIK_D)) { move.x += 1.0f; }

    // 通常移動方向を正規化
    if (Length(move) > 0.0f) {
        move = Normalize(move);
    }

    // 回避開始
    if (!isDodging_ && input->Triggerkey(DIK_LSHIFT)) {
        isDodging_ = true;
        dodgeTimer_ = dodgeDuration_;

        // 入力があればその方向、無ければ前方向
        if (Length(move) > 0.0f) {
            dodgeDirection_ = move;
        } else {
            dodgeDirection_ = { 0.0f, 0.0f, 1.0f };
        }
    }

    if (isDodging_) {
        pos_ += dodgeDirection_ * dodgeSpeed_;

        // くるっと回る
        rot_.x += 0.5f;

        dodgeTimer_--;
        if (dodgeTimer_ <= 0) {
            isDodging_ = false;
            rot_.x = 0.0f;
        }
    } else {
        // 通常移動
        if (Length(move) > 0.0f) {
            pos_ += move * moveSpeed_;
        }
    }
}