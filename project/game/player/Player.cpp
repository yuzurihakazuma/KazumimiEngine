#include "game/player/Player.h"
#include "Engine/Base/Input.h"
#include "engine/math/VectorMath.h"

using namespace VectorMath;

void Player::Initialize() {
    pos_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 1.0f, 1.0f, 1.0f };
}

void Player::Update() {
    Input* input = Input::GetInstance();

    Vector3 move{ 0.0f, 0.0f, 0.0f };

    if (input->Pushkey(DIK_W)) { move.z += 1.0f; }
    if (input->Pushkey(DIK_S)) { move.z -= 1.0f; }
    if (input->Pushkey(DIK_A)) { move.x -= 1.0f; }
    if (input->Pushkey(DIK_D)) { move.x += 1.0f; }

    if (Length(move) > 0.0f) {
        move = Normalize(move);
        pos_ += move * moveSpeed_;
    }
}