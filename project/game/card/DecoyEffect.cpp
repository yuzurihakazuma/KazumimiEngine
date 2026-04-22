#include "DecoyEffect.h"
#include <cmath>

using namespace VectorMath;

void DecoyEffect::Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera) {
	isFinished_ = false;
	timer_ = duration_;

	Vector3 forward = { std::sinf(casterYaw), 0.0f, std::cosf(casterYaw) };
	pos_ = { casterPos.x + forward.x * 2.0f, casterPos.y, casterPos.z + forward.z * 2.0f };

	model_ = SkinnedObj3d::Create("player", "resources/player", "player.gltf");
	if (model_) {
		model_->SetName("Decoy");
		model_->SetCamera(camera);
		model_->SetLoopAnimation(true);
		model_->SetIsWalking(false);
		model_->SetScale(scale_);
		model_->SetRotation({ 0.0f, casterYaw, 0.0f });
		model_->SetTranslation(pos_);
		model_->Update();
	}

}

void DecoyEffect::Update(Player *player, EnemyManager *enemyManager, Boss *boss,  const Vector3 &bossPos, const LevelData &level) {
	(void)player;
	(void)enemyManager;
	(void)boss;
	(void)bossPos;
	(void)level;

	if (isFinished_) {
		return;
	}

	timer_--;
	if (timer_ <= 0) {
		isFinished_ = true;
	}

	if (model_) {
		model_->SetIsWalking(false);
		model_->SetTranslation(pos_);
		model_->Update();
	}
}

void DecoyEffect::Draw() {
	if (!isFinished_ && model_) {
		model_->Draw();
	}
}
