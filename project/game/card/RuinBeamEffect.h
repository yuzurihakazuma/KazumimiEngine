#pragma once
#include "game/card/ICardEffect.h"
#include "engine/3d/obj/Obj3d.h"

#include <memory>

class RuinBeamEffect : public ICardEffect {
public:
    RuinBeamEffect(int damage) : damage_(damage) {}

    void Start(const Vector3& casterPos, float casterYaw, bool isPlayerCaster, Camera* camera) override;
    void Update(Player* player, EnemyManager* enemyManager, Boss* boss, const Vector3& bossPos, const LevelData& level) override;
    void Draw() override;

    bool IsFinished() const override { return isFinished_; }

private:
    bool IsPlayerInsideBeam(const Vector3& playerPos) const;

private:
    std::unique_ptr<Obj3d> obj_ = nullptr;
    Vector3 pos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 rot_ = { 0.0f, 0.0f, 0.0f };
    Vector3 baseScale_ = { 1.8f, 1.0f, 10.5f };

    int damage_ = 6;
    int lifeTimer_ = 72;
    int hitInterval_ = 12;
    int hitTimer_ = 0;
    bool isFinished_ = false;
};
