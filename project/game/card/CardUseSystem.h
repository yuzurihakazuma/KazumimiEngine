#pragma once
#include <memory>
#include <vector>
#include "engine/math/VectorMath.h"
#include "game/card/HandManager.h"
#include "engine/utils/Level/LevelEditor.h" // LevelDataを使うため
#include "engine/collision/Collision.h"     // AABB衝突判定用

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
	void Initialize(Camera *camera);

    // 更新
    void Update(Player* player, Enemy* enemy, Boss* boss,
        const Vector3& playerPos,
        const Vector3& enemyPos,
        const Vector3& bossPos,
        const LevelData& level);

	// 描画
	void Draw();

	// カード使用
	// card           : 使用するカード
	// casterPos      : 使用者の位置
	// casterYaw      : 使用者の向き
	// isPlayerCaster : プレイヤーが使ったかどうか
	// player         : プレイヤー本人（プレイヤー使用時の行動ロック用）
	void UseCard(const Card &card, const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Player *player = nullptr);

	// リセット
	void Reset();

private:
    // パンチ更新
    void UpdatePunch(Player* player, Enemy* enemy, Boss* boss,
        const Vector3& playerPos, const Vector3& enemyPos, const Vector3& bossPos);

    // 火球更新
    void UpdateFireball(Player* player, Enemy* enemy, Boss* boss,
        const Vector3& playerPos, const Vector3& enemyPos, const Vector3& bossPos,
        const LevelData& level);

	// ブロック衝突判定
	bool CheckBlockCollision(const Vector3 &pos, float radius, const LevelData &level);

	//シールド更新
	void UpdateShield(Player *player);

    //　氷の弾の更新関数
    void UpdateIceBullet(Player* player, Enemy* enemy, Boss* boss,
        const Vector3& playerPos, const Vector3& enemyPos, const Vector3& bossPos,
        const LevelData& level);

	// 地面からトゲ攻撃の更新間数
	void UpdateFangs(Player *player, Enemy *enemy, const Vector3 &enemyPos, const LevelData &level);

private:
	// -----------------------------
	// パンチ演出
	// -----------------------------
	std::unique_ptr<Obj3d> punchObj_ = nullptr;   // パンチ用オブジェクト
	bool isPunchActive_ = false;                  // パンチ演出中か
	bool isPunchPlayerCaster_ = true;             // プレイヤーが使ったパンチか
	Vector3 punchPos_ = { 0.0f, 0.0f, 0.0f };    // パンチ位置
	Vector3 punchScale_ = { 0.8f, 0.8f, 0.8f };  // パンチサイズ
	int punchTimer_ = 0;                         // パンチ残り時間

	// -----------------------------
	// 火球演出
	// -----------------------------
	std::unique_ptr<Obj3d> fireballObj_ = nullptr;      // 火球用オブジェクト
	bool isFireballActive_ = false;                     // 火球演出中か
	bool isFireballPlayerCaster_ = true;                // プレイヤーが使った火球か
	Vector3 fireballPos_ = { 0.0f, 0.0f, 0.0f };       // 火球位置
	Vector3 fireballVelocity_ = { 0.0f, 0.0f, 0.0f };  // 火球速度
	Vector3 fireballScale_ = { 0.5f, 0.5f, 0.5f };     // 火球サイズ

	// -----------------------------
	// シールド演出
	// -----------------------------
	std::unique_ptr<Obj3d> shieldObj_ = nullptr;      // シールド用オブジェクト
	Vector3 shieldScale_ = { 1.5f,1.5f,1.5f };          // プレイヤーをすっぽり囲うサイズ
	bool isShieldVisualActive_ = false;               // 描画フラグ

	// -----------------------------
	// シールド演出
	// -----------------------------
	std::unique_ptr<Obj3d> iceBulletObj_ = nullptr;     // 氷の弾用オブジェクト
	bool isIceBulletActive_ = false;                    // 氷の弾演出中か
	bool isIceBulletPLayerCaster_ = true;               // プレイヤーが使った氷の弾か
	Vector3 iceBulletPos_ = { 0.0f,0.0f,0.0f };         // 氷の弾位置
	Vector3 iceBulletVelocity_ = { 0.0f,0.0f,0.0f };    // 氷の弾速度
	Vector3 iceBulletScale_ = { 0.5f,0.5f,0.5f };       // 氷の弾サイズ

	// -----------------------------
	// 地面からトゲ攻撃
	// -----------------------------

	struct FangData {
		Vector3 pos;         // トゲの位置
		int delayTimer;      // 出現するまでの待ち時間
		int activeTimer;     // 出現している時間
		bool isActive;       // 今地面に出ているか
		bool hasHit;         // すでに敵に当たったか
	};

	std::vector<FangData> fangs_;
	std::unique_ptr<Obj3d> fangObj_ = nullptr;
	bool isFangsAttackActive_ = false;
	bool isFangsPlayerCaster_ = true;


};
