#include "SpeedBuffEffect.h"
#include "game/player/Player.h"

void SpeedBuffEffect::Start(const Vector3 &casterPos, float casterYaw, bool isPLayerCaster, Camera *camera) {
	isPlayerCaster_ = isPLayerCaster;
	isFinished_ = false;
}

void SpeedBuffEffect::Update(Player *player, EnemyManager *enemyManager, Boss *boss,  const Vector3 &bossPos, const LevelData &level) {

	if (isFinished_) {
		return;
	}

	if (isPlayerCaster_ && player != nullptr && !player->IsDead()) {
		//300フレーム速度アップ
		player->ApplySpeedBuff(multiplier_, 300);
	}

	// これも一瞬で終わるので、即時に終了フラグを立てる
	isFinished_ = true;

}

void SpeedBuffEffect::Draw() {
}
