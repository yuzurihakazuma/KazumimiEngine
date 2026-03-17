#pragma once
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>
#include "engine/math/VectorMath.h"
#include "game/card/HandManager.h"
#include "engine/utils/Level/LevelEditor.h"
#include "engine/collision/Collision.h"
#include "game/card/ICardEffect.h"

// 前方宣言
class Camera;
class Obj3d;
class Player;
class Enemy;
class Boss;

// カード使用システム
class CardUseSystem {
public:
	// 初期化
	void Initialize(Camera* camera);

	// 更新
	void Update(Player* player, Enemy* enemy, Boss* boss,
		const Vector3& playerPos,
		const Vector3& enemyPos,
		const Vector3& bossPos,
		const LevelData& level);

	// 描画
	void Draw();

	// カード使用（ここでは即発動ではなく詠唱開始）
	void UseCard(const Card& card, const Vector3& casterPos, float casterYaw, bool isPlayerCaster, Player* player = nullptr);

	// リセット
	void Reset();

	void CancelCasting();


	bool IsDecoyActive() const;
	Vector3 GetDecoyPosition() const;

private:
	// 実際のカード発動処理
	void ExecuteCard(const Card& card, const Vector3& casterPos, float casterYaw, bool isPlayerCaster, Player* player);

	// カードごとの詠唱時間
	int GetCastTime(const Card& card) const;

	

	// 「カードID」と「魔法を生み出す関数」の辞書
	std::unordered_map<int, std::function<std::unique_ptr<ICardEffect>(const Card &)>> effectFactory_;

private:
	// -----------------------------
	// 詠唱状態
	// -----------------------------
	bool isCasting_ = false;
	Card castingCard_{ -1, "", 0 };
	Vector3 castPos_{ 0.0f, 0.0f, 0.0f };
	float castYaw_ = 0.0f;
	bool isPlayerCasting_ = true;
	int castTimer_ = 0;
	Player* castingPlayer_ = nullptr;
	static constexpr int kCommonCastTime_ = 20;

	


	// カメラを保存しておくための変数
	Camera *camera_ = nullptr;

	// 現在進行中の魔法リスト
	std::vector<std::unique_ptr<ICardEffect>> activeEffects_;
};