#include "HealEffect.h"
#include "player/Player.h"

void HealEffect::Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera) {
	isPlayerCaster_ = isPlayerCaster;
	isFinished_ = false;
}

void HealEffect::Update(Player *player, Enemy *enemy, Boss *boss, const Vector3 &enemyPos, const Vector3 &bossPos, const LevelData &level) {
	if (isFinished_) {
		return;
	}

	// プレイヤーが使った場合のみ回復する
	if (isPlayerCaster_ && player != nullptr && !player->IsDead()) {
		player->Heal(healAmount_); // 受け取った回復量で回復
	}

	// 回復は一瞬で終わるので、即時終了フラグを立てる
	isFinished_ = true;

}

void HealEffect::Draw() {

}
