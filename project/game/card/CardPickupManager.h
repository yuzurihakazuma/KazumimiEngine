#pragma once
#include <vector>
#include "game/card/CardPickup.h"

class Camera;

class CardPickupManager {
public:
    void Initialize(Camera* camera);
    void Update();
    void Draw();

    void AddPickup(const Vector3& position, const Card& card);

    std::vector<CardPickup>& GetPickups() { return pickups_; }
    const std::vector<CardPickup>& GetPickups() const { return pickups_; }

private:
    std::vector<CardPickup> pickups_;
    Camera* camera_ = nullptr;
};