#include "DecoyEffect.h"
#include <cmath>

using namespace VectorMath;

void DecoyEffect::Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera) {
	isFinished_ = false;
	timer_ = duration_;

	Vector3 forward = { std::sinf(casterYaw), 0.0f, std::cosf(casterYaw) };
	pos_ = { casterPos.x + forward.x * 2.0f, casterPos.y, casterPos.z + forward.z * 2.0f };

	obj_ = Obj3d::Create("sphere"); 
	if (obj_) {
		obj_->SetCamera(camera);
		obj_->SetScale(scale_);
		obj_->SetTranslation(pos_);
		obj_->Update();
	}

}

void DecoyEffect::Update(Player *player, EnemyManager *enemyManager, Boss *boss,  const Vector3 &bossPos, const LevelData &level) {
	if (isFinished_) {
		return;
	}

	timer_--;
	if (timer_ <= 0) {
		isFinished_ = true;
	}

	if (obj_) {
		obj_->SetTranslation(pos_);
		obj_->Update();
	}
}

void DecoyEffect::Draw() {
	if (!isFinished_ && obj_) {
		obj_->Draw();
	}
}
