#pragma once
#include <memory>
#include "engine/math/VectorMath.h"
#include "game/card/HandManager.h"

class Obj3d;

struct CardPickup {
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    Card card;
    bool isActive = true;

    std::unique_ptr<Obj3d> obj = nullptr;
};