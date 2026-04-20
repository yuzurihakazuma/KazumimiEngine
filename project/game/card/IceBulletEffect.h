#pragma once
#include "game/card/ICardEffect.h"
#include "engine/3d/obj/Obj3d.h"
#include <memory>

class IceBulletEffect : public ICardEffect {
public:
	// 効果開始
	void Start(const Vector3& casterPos, float casterYaw, bool isPlayerCaster, Camera* camera) override;

	// 毎フレーム更新
	void Update(Player* player, EnemyManager *enemyManager, Boss* boss,
		 const Vector3& bossPos, const LevelData& level) override;

	// 描画
	void Draw() override;

	// 終了判定
	bool IsFinished() const override { return isFinished_; }

private:
	std::unique_ptr<Obj3d> obj_ = nullptr;    // 表示用オブジェクト
	Vector3 pos_ = { 0.0f, 0.0f, 0.0f };      // 位置
	Vector3 velocity_ = { 0.0f, 0.0f, 0.0f }; // 移動速度
	Vector3 scale_ = { 0.5f, 0.5f, 0.5f };    // 弾の大きさ

	bool isPlayerCaster_ = true;              // 使用者がプレイヤーか
	bool isFinished_ = false;                 // 終了フラグ

	float rotAngle_ = 0.0f;
	int sparkTimer_ = 0;

};