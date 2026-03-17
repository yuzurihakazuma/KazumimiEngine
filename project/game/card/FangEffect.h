#pragma once
#include "game/card/ICardEffect.h"
#include "engine/3d/obj/Obj3d.h"
#include <memory>
#include <vector>

// トゲ1本ごとのデータ
struct FangData {
	Vector3 pos;
	int delayTimer;
	int activeTimer;
	bool isActive;
	bool hasHit;
};

class FangEffect : public ICardEffect {
public:

	// 初期化
	void Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera)override;

	// 更新
	void Update(Player *player, Enemy *enemy, Boss *boss, const Vector3 &enemyPos, const Vector3 &bossPos, const LevelData &level)override;

	// 描画
	void Draw()override;

	bool IsFinished()const override { return isFinished_; }

private:
	std::unique_ptr<Obj3d> obj_ = nullptr;
	std::vector<FangData> fangs_;
	Vector3 scale_ = { 0.5f,2.0f,0.5f };
	bool isPlayerCaster_ = true;
	bool isFinished_ = false;
};

