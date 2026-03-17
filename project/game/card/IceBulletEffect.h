#pragma once
#include "game/card/ICardEffect.h"
#include "engine/3d/obj/Obj3d.h"
#include <memory>

class IceBulletEffect : public ICardEffect {
public:
	
	// 初期化
	void Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera) override;
	
	// 更新
	void Update(Player *player, Enemy *enemy, Boss *boss, const Vector3 &enemyPos, const Vector3 &bossPos, const LevelData &level) override;
	
	// 描画
	void Draw() override;
	
	bool IsFinished() const override { return isFinished_; }

private:
	std::unique_ptr<Obj3d> obj_ = nullptr;
	Vector3 pos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 velocity_ = { 0.0f, 0.0f, 0.0f };
	Vector3 scale_ = { 0.5f, 0.5f, 0.5f }; // 氷の弾のサイズ
	bool isPlayerCaster_ = true;
	bool isFinished_ = false;
};

