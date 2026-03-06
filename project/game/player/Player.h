#pragma once
#include "engine/math/VectorMath.h"

class Player {
public:
    void Initialize();
    void Update();

    const Vector3& GetPosition() const { return pos_; }
    const Vector3& GetScale() const { return scale_; }

    void SetPosition(const Vector3& pos) { pos_ = pos; }
    void SetScale(const Vector3& scale) { scale_ = scale; }

private:
    Vector3 pos_{ 0.0f, 0.0f, 0.0f };
    Vector3 scale_{ 1.0f, 1.0f, 1.0f };

    float moveSpeed_ = 0.2f;
};