#pragma once
#include "engine/math/VectorMath.h"
#include "engine/utils/Level/LevelEditor.h"

// 前方宣言
class  Camera;
class Player;
class Enemy;
class Boss;

// 全てのカード効果のベースとなるクラス
class ICardEffect {
public:
	virtual ~ICardEffect() = default;

	// カード効果の初期化処理
	virtual void Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *caemra) = 0;

	// 毎フレームの更新処理
	virtual void Update(Player *player, Enemy *enemy, Boss *boss, const Vector3 &enemyPos, const Vector3 &bossPos, const LevelData &level) = 0;

	// 描画処理
	virtual void Draw() = 0;

	// このカードの演出が終わったかどうか（終わったらシステムから削除するため）
	virtual bool IsFinished() const = 0;
};