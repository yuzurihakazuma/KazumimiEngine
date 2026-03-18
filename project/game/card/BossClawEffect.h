#pragma once
#include "game/card/ICardEffect.h"
#include "engine/3d/obj/Obj3d.h"
#include <memory>

class BossClawEffect : public ICardEffect {
public:

	// コンストラクタでダメージ(CSVのeffectValue)を受け取る
	BossClawEffect(int damage) : damage_(damage) {}

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
	Vector3 scale_ = { 2.0f, 2.0f, 2.0f }; // プレイヤーの爪と同じサイズ

	int damage_ = 20;     // ダメージ量
	int timer_ = 0;       // 演出の進行タイマー
	bool hasHit_ = false; // 現在の攻撃が当たったかどうか
	bool isFinished_ = false;
};

