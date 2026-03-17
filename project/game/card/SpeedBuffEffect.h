#pragma once
#include "game/card/ICardEffect.h"

class SpeedBuffEffect : public ICardEffect {
public:

	// コンストラクタで倍率を受け取る
	SpeedBuffEffect(float multiplier) : multiplier_(multiplier) {}

	// 初期化
	void Start(const Vector3 &casterPos, float casterYaw, bool isPLayerCaster, Camera *camera)override;

	// 更新
	void Update(Player *player, Enemy *enemy, Boss *boss, const Vector3 &enemyPos, const Vector3 &bossPos, const LevelData &level)override;

	// 描画
	void Draw()override;

	bool IsFinished()const override { return isFinished_; }


private:
	float multiplier_ = 1.0f; // 速度倍率
	bool isPlayerCaster_ = true;
	bool isFinished_ = false;
};

