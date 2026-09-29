#include "game/player/PlayerInput.h"
#include "engine/base/Input.h"

#include <cmath>

// カメラの向き(90°単位)に応じた「実キー → ワールド方向」の割り当て。
//   通常（カメラが後ろ・0°）は D=+X / A=-X / W=+Z / S=-Z。
//   カメラが正面へ回り込んだ(180°)ら D=-X / W=-Z … と割り当てごと回すので、
//   どの向きでも「押したキーの方向＝画面で進む方向」が一致する。
//   ※連続角で合成せず90°に量子化することで、既存の移動ロジック（進行符号等）を一切変えずに済む。
PlayerInput::WorldKeys PlayerInput::GetWorldKeys() const{
    // camYaw_ は「カメラが見ている方向」の yaw（camera->GetRotation().y）。
    //   画面の奥(W) = 注視方向 (sinθ, cosθ) / 画面の右(D) = (cosθ, -sinθ)。
    //   θ=+90°: W=+X, D=-Z / θ=-90°: W=-X, D=+Z（±90°を取り違えないこと！）
    const float kHalfPi = 1.57079632f;
    int rawQuadrant = ( int ) std::lround(camYaw_ / kHalfPi);

    // ヒステリシス：90°の中心から大きく外れている（＝カメラが回転の途中）間は
    // 前回の割り当てを維持する。境界(45°)付近で毎フレーム切り替わるのを防ぐ。
    float angleDiff = std::abs(camYaw_ - rawQuadrant * kHalfPi);
    int quadrant;
    if ( angleDiff < 0.6f ) { // 中心から約34°以内なら確定
        quadrant = ( ( rawQuadrant % 4 ) + 4 ) % 4;
        lastKeyQuad_ = quadrant;
    } else {
        quadrant = lastKeyQuad_; // 回転途中は前回の向きのまま
    }

    switch ( quadrant ) {
    case 1:  return { DIK_W, DIK_S, DIK_A, DIK_D }; // 注視+90°（左側から見る）
    case 2:  return { DIK_A, DIK_D, DIK_S, DIK_W }; // 正面(180°)＝完全反転
    case 3:  return { DIK_S, DIK_W, DIK_D, DIK_A }; // 注視-90°（右側から見る）
    default: return { DIK_D, DIK_A, DIK_W, DIK_S }; // 後ろから(0°)
    }
}

PlayerInput::RailInput PlayerInput::ReadRail(bool horizontalRail) const{
    Input* input = Input::GetInstance();
    const WorldKeys keys = GetWorldKeys();
    // 横レールは X が移動・Z が乗り換え、縦レールはその逆
    const int movePlus    = horizontalRail ? keys.plusX  : keys.plusZ;
    const int moveMinus   = horizontalRail ? keys.minusX : keys.minusZ;
    const int switchPlus  = horizontalRail ? keys.plusZ  : keys.plusX;
    const int switchMinus = horizontalRail ? keys.minusZ : keys.minusX;

    RailInput result;
    if ( input->Pushkey(( BYTE ) movePlus) )      result.move      += 1.0f;
    if ( input->Pushkey(( BYTE ) moveMinus) )     result.move      -= 1.0f;
    if ( input->Triggerkey(( BYTE ) switchPlus) )  result.switchDir += 1; // 横レールなら奥(+Z)、縦レールなら右(+X)へ
    if ( input->Triggerkey(( BYTE ) switchMinus) ) result.switchDir -= 1;
    return result;
}

float PlayerInput::ReadAirForward(const Vector3& forwardDir) const{
    Input* input = Input::GetInstance();
    const WorldKeys keys = GetWorldKeys();
    float inputX = 0.0f, inputZ = 0.0f;
    if ( input->Pushkey(( BYTE ) keys.plusX) )  inputX += 1.0f;
    if ( input->Pushkey(( BYTE ) keys.minusX) ) inputX -= 1.0f;
    if ( input->Pushkey(( BYTE ) keys.plusZ) )  inputZ += 1.0f;
    if ( input->Pushkey(( BYTE ) keys.minusZ) ) inputZ -= 1.0f;
    return inputX * forwardDir.x + inputZ * forwardDir.z;
}

bool PlayerInput::JumpPressed() const{ return Input::GetInstance()->Triggerkey(DIK_SPACE); }
bool PlayerInput::JumpHeld() const{ return Input::GetInstance()->Pushkey(DIK_SPACE); }
