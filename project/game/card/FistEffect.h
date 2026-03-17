#pragma once
#include "game/card/ICardEffect.h"

class FistEffect : public ICardEffect {
public:
	// カード効果の初期化処理
	void Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera)override;

	// 毎フレームの更新処理
	void Update(Player *player, Enemy *enemy, Boss *boss, const Vector3 &enemyPos, const Vector3 &bossPos, const LevelData &level) override;

	// 描画処理
	void Draw() override;

	// カードの演出が終わったかどうか
	bool IsFinished() const override { return isFinished_; }
private:
	std::unique_ptr<Obj3d> obj_ = nullptr;    // 3Dオブジェクト
	Vector3 pos_ = { 0.0f,0.0f,0.0f };        // 位置
	Vector3 velocity_ = { 0.0f,0.0f,0.0f };   // 速度
	Vector3 scale_ = { 0.8f,0.8f,0.8f };      // スケール
	bool isPlayerCaster_ = true;
	bool isFinished_ = false;
	int punchTimer_ = 0;
};

