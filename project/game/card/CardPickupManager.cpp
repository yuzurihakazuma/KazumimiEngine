#include "game/card/CardPickupManager.h"
#include "Engine/3D/Obj/Obj3d.h"
#include "Engine/Camera/Camera.h"

void CardPickupManager::Initialize(Camera* camera) {
    camera_ = camera;
    pickups_.clear();
}

void CardPickupManager::AddPickup(const Vector3& position, const Card& card) {
    CardPickup newPickup;
    newPickup.position = position;
    newPickup.card = card;
    newPickup.isActive = true;

    newPickup.obj = Obj3d::Create("cardR");
    if (newPickup.obj) {
        newPickup.obj->SetCamera(camera_);
        newPickup.obj->SetTranslation(newPickup.position);
        newPickup.obj->SetScale({ 1.0f, 1.5f, 1.0f });
        newPickup.obj->SetRotation({ 1.57f, 0.0f, 0.0f });
        newPickup.obj->Update();
    }

    pickups_.push_back(std::move(newPickup));
}

void CardPickupManager::Update() {
    for (auto& pickup : pickups_) {
        if (!pickup.isActive) {
            continue;
        }

        if (pickup.obj) {
            pickup.obj->SetTranslation(pickup.position);
            pickup.obj->Update();
        }
    }
}

void CardPickupManager::Draw() {
    for (auto& pickup : pickups_) {
        if (!pickup.isActive) {
            continue;
        }

        if (pickup.obj) {
            pickup.obj->Draw();
        }
    }
}