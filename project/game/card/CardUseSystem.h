#pragma once
#include <memory>
#include <vector>
#include "engine/math/VectorMath.h"
#include "game/card/HandManager.h"
#include "engine/utils/Level/LevelEditor.h"
#include "engine/collision/Collision.h"

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

	// 身代わりが出現中か
	bool IsDecoyActive() const { return isDecoyActive_; }

	// 身代わりの位置取得
	const Vector3 &GetDecoyPosition() const { return decoyPos_; }

private:
	// 実際のカード発動処理
	void ExecuteCard(const Card& card, const Vector3& casterPos, float casterYaw, bool isPlayerCaster, Player* player);

	// カードごとの詠唱時間
	int GetCastTime(const Card& card) const;

	// パンチ更新
	void UpdatePunch(Player* player, Enemy* enemy, Boss* boss,
		const Vector3& playerPos, const Vector3& enemyPos, const Vector3& bossPos);

	// 火球更新
	void UpdateFireball(Player* player, Enemy* enemy, Boss* boss,
		const Vector3& playerPos, const Vector3& enemyPos, const Vector3& bossPos,
		const LevelData& level);

	// ブロック衝突判定
	bool CheckBlockCollision(const Vector3& pos, float radius, const LevelData& level);

	// シールド更新
	void UpdateShield(Player* player);

	// 氷の弾の更新
	void UpdateIceBullet(Player* player, Enemy* enemy, Boss* boss,
		const Vector3& playerPos, const Vector3& enemyPos, const Vector3& bossPos,
		const LevelData& level);

	// 地面からトゲ攻撃の更新間数
	void UpdateFangs(Player *player, Enemy *enemy, Boss *boss, const Vector3 &enemyPos, const Vector3 &bossPos, const LevelData &level);

	// 身代わりの更新関数
	void UpdateDecoy();

	// 攻撃力ダウンの更新関数
	void UpdateAtkDown(Enemy *enemy, Boss *boss, const Vector3 &enemyPos, const Vector3 &bossPos);

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

	// -----------------------------
	// パンチ演出
	// -----------------------------
	std::unique_ptr<Obj3d> punchObj_ = nullptr;
	bool isPunchActive_ = false;
	bool isPunchPlayerCaster_ = true;
	Vector3 punchPos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 punchScale_ = { 0.8f, 0.8f, 0.8f };
	int punchTimer_ = 0;

	// -----------------------------
	// 火球演出
	// -----------------------------
	std::unique_ptr<Obj3d> fireballObj_ = nullptr;
	bool isFireballActive_ = false;
	bool isFireballPlayerCaster_ = true;
	Vector3 fireballPos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 fireballVelocity_ = { 0.0f, 0.0f, 0.0f };
	Vector3 fireballScale_ = { 0.5f, 0.5f, 0.5f };

	// -----------------------------
	// シールド演出
	// -----------------------------
	std::unique_ptr<Obj3d> shieldObj_ = nullptr;
	Vector3 shieldScale_ = { 1.5f,1.5f,1.5f };
	bool isShieldVisualActive_ = false;

	// -----------------------------
	// 氷の弾演出
	// -----------------------------
	std::unique_ptr<Obj3d> iceBulletObj_ = nullptr;
	bool isIceBulletActive_ = false;
	bool isIceBulletPLayerCaster_ = true;
	Vector3 iceBulletPos_ = { 0.0f,0.0f,0.0f };
	Vector3 iceBulletVelocity_ = { 0.0f,0.0f,0.0f };
	Vector3 iceBulletScale_ = { 0.5f,0.5f,0.5f };

	// -----------------------------
	// 地面からトゲ攻撃
	// -----------------------------
	struct FangData {
		Vector3 pos;
		int delayTimer;
		int activeTimer;
		bool isActive;
		bool hasHit;
	};

	std::vector<FangData> fangs_;
	std::unique_ptr<Obj3d> fangObj_ = nullptr;
	bool isFangsAttackActive_ = false;
	bool isFangsPlayerCaster_ = true;

	// -----------------------------
	// 身代わり（デコイ）演出
	// -----------------------------

	std::unique_ptr<Obj3d> decoyObj_ = nullptr; // 身代わりのモデル
	bool isDecoyActive_ = false; // 身代わりの出現中か
	Vector3 decoyPos_ = { 0.0f,0.0f,0.0f }; // 身代わりの位置
	Vector3 decoyScale_ = { 1.0f,1.0f,1.0f }; // 身代わりの大きさ
	int decoyTimer_ = 0; // 身代わりの残り寿命タイマー

	// -----------------------------
	// 攻撃力ダウン演出
	// -----------------------------
	std::unique_ptr<Obj3d> atkDownObj_ = nullptr;
	bool isAtkDownActive_ = false;
	bool isAtkDownPlayerCaster_ = true;
	Vector3 atkDownPos_ = { 0.0f, 0.0f, 0.0f };
	int atkDownTimer_ = 0;

};