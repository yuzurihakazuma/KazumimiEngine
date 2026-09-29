#pragma once
// =====================================================================
//  PlayerInput：プレイヤーの入力（キー → レール上の移動・乗り換え・空中の前後・ジャンプ）。
//   キーは「ワールド方向の意思」で読む（D=世界+X / W=世界+Z）。
//   カメラが回り込んだ時（180°向き切替など）は割り当てごと90°単位で回すので、
//   どの向きでも「Dを押せば画面の右へ進む」。
// =====================================================================
#include "engine/math/struct.h"

class PlayerInput {
public:
    // カメラの向き(yaw)。シーンが毎フレーム渡す
    void SetCameraYaw(float yawRad){ camYaw_ = yawRad; }

    // レール上の入力。横レール … ±Xキー=移動 / ±Zキー=縦レールへ乗り換え
    //               縦レール … ±Zキー=移動 / ±Xキー=横レールへ乗り換え
    struct RailInput {
        float move = 0.0f;   // 移動（±1。レールの軸方向のワールド向き）
        int   switchDir = 0; // 乗り換え（±1。押した瞬間だけ）
    };
    RailInput ReadRail(bool horizontalRail) const;

    // 空中の前後入力：押しているワールド方向を、空中に出た時の進行方向 forwardDir へ射影した値（+前/-後）
    float ReadAirForward(const Vector3& forwardDir) const;

    bool JumpPressed() const; // SPACE を押した瞬間
    bool JumpHeld() const;    // SPACE を押している間

private:
    // カメラの向き(90°単位)に応じた「実キー → ワールド方向」の割り当て（DIK_～ のキーコード）
    struct WorldKeys { int plusX, minusX, plusZ, minusZ; };
    WorldKeys GetWorldKeys() const;

    float camYaw_ = 0.0f;
    // 直近に確定したキー割り当ての向き(0〜3)。カメラ回転の途中は前回を維持（ちらつき防止）
    mutable int lastKeyQuad_ = 0;
};
