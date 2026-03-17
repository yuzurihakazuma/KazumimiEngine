#pragma once
#include "game/card/ICardEffect.h"
#include "engine/3d/obj/Obj3d.h"
#include <memory>

class ShieldEffect : public ICardEffect {
public:
	// 持続時間をコンストラクタで受け取る
	ShieldEffect(int duration) : duration_(duration) {}

	// 初期化
	void Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera)override;

	// 更新
	void Update(Player *player, Enemy *enemy, Boss *boss, const Vector3 &enmeyPos, const Vector3 &bossPos, const LevelData &level)override;

	// 描画
	void Draw()override;

	bool IsFinished()const override { return isFinished_; }

private:
	std::unique_ptr<Obj3d> obj_ = nullptr;
	Vector3 scale_ = { 1.5f,1.5f,1.5f }; // シールドの大きさ
	int duration_ = 300; // シールドの持続時間
	int timer_ = 0;
	bool isPlayerCaster_ = true;
	bool isFinished_ = false;
};

