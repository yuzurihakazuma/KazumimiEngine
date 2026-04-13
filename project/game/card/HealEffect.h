#pragma once
#include "game/card/ICardEffect.h"

class HealEffect : public ICardEffect{
public:

	// コンストラクタで回復量を受け取る
	HealEffect(int healAmount) : healAmount_(healAmount) {}

	// 初期化
	void Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera)override;

	// 更新
	void Update(Player *player, EnemyManager *enemyManager, Boss *boss,  const Vector3 &bossPos, const LevelData &level)override;

	// 描画
	void Draw()override;

	bool IsFinished() const override { return isFinished_; }

private:
	int healAmount_ = 5; // 回復量
	bool isPlayerCaster_ = true;
	bool isFinished_ = false;

};

