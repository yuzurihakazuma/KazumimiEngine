#include "FistEffect.h"
#include "Engine/3D/Obj/Obj3d.h"
#include "game/player/Player.h"
#include "game/enemy/Enemy.h"
#include "game/enemy/Boss.h"
#include "engine/math/VectorMath.h"
#include <memory>

using namespace VectorMath;

void FistEffect::Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera) {

	
	isPlayerCaster_ = isPlayerCaster;
	punchTimer_ = 10;
	
	// 前方に進むベクトルを計算
	Vector3 forward = {
		std::sinf(casterYaw),
		0.0f,
		std::cosf(casterYaw)
	};

	pos_ = {
		casterPos.x + forward.x * 1.5f,
		casterPos.y,
		casterPos.z + forward.z * 1.5f
	};

	// パンチ用オブジェクト生成
	obj_ = Obj3d::Create("sphere");
	if (obj_) {
		obj_->SetCamera(camera);
		obj_->SetScale(scale_);
		obj_->SetTranslation(pos_);
		obj_->Update();
	}
}

void FistEffect::Update(Player *player, Enemy *enemy, Boss *boss, const Vector3 &enemyPos, const Vector3 &bossPos, const LevelData &level) {
		if (isFinished_) {
			return;
		}

		// タイマーを減らし、0になったら演出終了
		punchTimer_--;
		if (punchTimer_ <= 0) {
			isFinished_ = true;
			return;
		}

		// オブジェクトの更新
		if (obj_) {
			obj_->SetTranslation(pos_);
			obj_->SetScale(scale_);
			obj_->Update();
		}

		if (isPlayerCaster_) {

			if (enemy && !enemy->IsDead()) {
				Vector3 diff = {
					enemyPos.x - pos_.x,
					enemyPos.y - pos_.y,
					enemyPos.z - pos_.z
				};

				if (Length(diff) < 2.0f) {
					enemy->TakeDamage(1);
					isFinished_ = true; // 当たったらパンチの判定を消す
					return;

				}
			}

			if (boss && !boss->IsDead()) {
				Vector3 diff = {
					bossPos.x - pos_.x,
					bossPos.y - pos_.y,
					bossPos.z - pos_.z
				};

				if (Length(diff) < 2.5f) {
					boss->TakeDamage(1);
					isFinished_ = true;
					return;
				}
			}
		} else {
			if (player && !player->IsDead()) {
				Vector3 playerPos = player->GetPosition();
				Vector3 diff = {
					playerPos.x - pos_.x,
					playerPos.y - pos_.y,
					playerPos.z - pos_.z
				};

				if (Length(diff) < 2.0f) {
					int damage = 1;

					// 使用した敵がデバフ状態ならダメージを減らす（0にする）
					if (enemy && enemy->IsAttackDebuffed()) {
						damage = 0;
					} else if (boss && boss->IsAttackDebuffed()) {
						damage = 0;
					}

					player->TakeDamage(damage, pos_);

					isFinished_ = true;
					return;
				}
			}
		}
}

void FistEffect::Draw() {

	// パンチ描画
	if (!isFinished_ && obj_) {
		obj_->Draw();
	}
}
