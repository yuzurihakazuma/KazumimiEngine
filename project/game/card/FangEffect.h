#pragma once
#include "game/card/ICardEffect.h"
#include "engine/3d/obj/Obj3d.h"
#include <memory>
#include <vector>

// トゲ1本分の情報
struct FangData {
	Vector3 pos;       // 出現位置
	int delayTimer;    // 出現までの待機時間
	int activeTimer;   // 出現中の残り時間
	bool isActive;     // 現在有効か
	bool hasHit;       // 既にヒットしたか
};

class FangEffect : public ICardEffect {
public:
	// 効果開始
	void Start(const Vector3& casterPos, float casterYaw, bool isPlayerCaster, Camera* camera) override;

	// 毎フレーム更新
	void Update(Player* player, EnemyManager *enemyManager, Boss* boss,
		const Vector3& enemyPos, const Vector3& bossPos, const LevelData& level) override;

	// 描画
	void Draw() override;

	// 終了判定
	bool IsFinished() const override { return isFinished_; }

private:
	std::unique_ptr<Obj3d> obj_ = nullptr; // 表示用オブジェクト
	std::vector<FangData> fangs_;         // トゲの配列
	Vector3 scale_ = { 0.5f, 2.0f, 0.5f };// トゲの大きさ

	bool isPlayerCaster_ = true;          // 使用者がプレイヤーか
	bool isFinished_ = false;             // 効果終了フラグ
};