#pragma once
#include "engine/math/VectorMath.h"

class Player {
public:
    void Initialize();
    void Update();

    const Vector3& GetPosition() const { return pos_; }
    const Vector3& GetScale() const { return scale_; }
    const Vector3& GetRotation() const { return rot_; }

    void SetPosition(const Vector3& pos) { pos_ = pos; }
    void SetScale(const Vector3& scale) { scale_ = scale; }
    void SetRotation(const Vector3& rot) { rot_ = rot; }

private:
    Vector3 pos_{ 0.0f, 0.0f, 0.0f };
    Vector3 rot_{ 0.0f, 0.0f, 0.0f };
    Vector3 scale_{ 1.0f, 1.0f, 1.0f };

    float moveSpeed_ = 0.2f;

    // 回避用
    bool isDodging_ = false;
    int dodgeTimer_ = 0;
    const int dodgeDuration_ = 15;
    float dodgeSpeed_ = 0.6f;
    Vector3 dodgeDirection_{ 0.0f, 0.0f, 0.0f };
};