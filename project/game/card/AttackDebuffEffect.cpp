#include "AttackDebuffEffect.h"
#include "enemy/Enemy.h"
#include "enemy/Boss.h"

void AttackDebuffEffect::Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera) {
	isPlayerCaster_ = isPlayerCaster;
	isFinished_ = false;
}

void AttackDebuffEffect::Update(Player *player, Enemy *enemy, Boss *boss, const Vector3 &enemyPos, const Vector3 &bossPos, const LevelData &level) {
	if (isFinished_) {
		return;
	}

	// プレイヤーが使った場合、敵全員にデバフを付与
	if (isPlayerCaster_) {
		if (enemy && !enemy->IsDead()) {
			enemy->ApplyAttackDebuff(duration_); // ※Enemyクラスに関数を作っておく必要があります
		}
		if (boss && !boss->IsDead()) {
			boss->ApplyAttackDebuff(duration_);  // ※Bossクラスに関数を作っておく必要があります
		}
	}

	isFinished_ = true; // 1フレームで終了
}
