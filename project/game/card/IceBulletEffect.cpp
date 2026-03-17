#include "IceBulletEffect.h"
#include "game/player/Player.h"
#include "game/enemy/Enemy.h"
#include "game/enemy/Boss.h"
#include "engine/math/VectorMath.h"
#include "engine/collision/Collision.h"
#include <cmath>

using namespace VectorMath;

void IceBulletEffect::Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera) {
	isPlayerCaster_ = isPlayerCaster;
	isFinished_ = false;
	
	Vector3 forward = { std::sinf(casterYaw),0.0f,std::cosf(casterYaw) };

	pos_ = {
		casterPos.x + forward.x * 1.5f,
		casterPos.y,
		casterPos.z + forward.z * 1.5f
	};

	velocity_ = {
		forward.x * 0.4f,
		0.0f,
		forward.z * 0.4f
	};

	obj_ = Obj3d::Create("sphere"); // 氷用のモデル名があれば変更
	if (obj_) {
		obj_->SetCamera(camera);
		obj_->SetScale(scale_);
		obj_->SetTranslation(pos_);
		obj_->Update();
	}
}

void IceBulletEffect::Update(Player *player, Enemy *enemy, Boss *boss, const Vector3 &enemyPos, const Vector3 &bossPos, const LevelData &level) {

	if (isFinished_) {
		return;
	}

	pos_ += velocity_;

	// 壁にあったら消滅
	if (Collision::CheckBlockCollision(pos_, 0.5f, level)) {
		isFinished_ = true;
		return;
	}

	if (obj_) {
		obj_->SetTranslation(pos_);
		obj_->Update();
	}

	if (isPlayerCaster_) {
		// 敵への当たり判定
		if (enemy && !enemy->IsDead()) {
			Vector3 diff = { enemyPos.x - pos_.x,enemyPos.y - pos_.y,enemyPos.z - pos_.z };
			if (Length(diff) < 1.5f) {
				enemy->TakeDamage(1); // 氷の弾のダメージ
				isFinished_ = true;
				return;
			}
		}

		// ボスへの当たり判定
		if (boss && !boss->IsDead()) {
			Vector3 diff = { bossPos.x - pos_.x, bossPos.y - pos_.y, bossPos.z - pos_.z };
			if (Length(diff) < 2.0f) {
				boss->TakeDamage(1);
				isFinished_ = true;
				return;
			}
		}

		// 画面外（遠く）に行ったら消す
		if (pos_.x > 100.0f || pos_.x < -100.0f || pos_.z > 100.0f || pos_.z < -100.0f) {
			isFinished_ = true;
		}
	} else {
		// 敵が使った場合
		if (player && !player->IsDead()) {
			Vector3 diff = { player->GetPosition().x - pos_.x, player->GetPosition().y - pos_.y, player->GetPosition().z - pos_.z };
			if (Length(diff) < 1.5f) {
				int damage = 2; // 氷の弾の基本ダメージ
				if (enemy && enemy->IsAttackDebuffed()) damage = 1;
				else if (boss && boss->IsAttackDebuffed()) damage = 1;

				player->TakeDamage(damage, pos_);
				isFinished_ = true;
				return;
			}
		}
	}

}

void IceBulletEffect::Draw() {
	if (!isFinished_ && obj_) {
		obj_->Draw();
	}
}
