#include "FangEffect.h"
#include "game/player/Player.h"
#include "game/enemy/Enemy.h"
#include "game/enemy/Boss.h"
#include "engine/collision/Collision.h"
#include "engine/math/VectorMath.h"
#include <cmath>

using namespace VectorMath;

void FangEffect::Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera) {

	isPlayerCaster_ = isPlayerCaster;
	isFinished_ = false;

	Vector3 forward = { std::sinf(casterYaw),0.0f,std::cosf(casterYaw) };

	// 3本のトゲを均等間隔に配置
	for (int i = 0; i < 5; i++) {
		FangData fang;
		fang.pos = {
			casterPos.x + forward.x * (2.0f + i * 2.0f),
			casterPos.y,
			casterPos.z + forward.z * (2.0f + i * 2.0f)
		};

		fang.delayTimer = i * 5; // 10フレームずつ遅らせて出現
		fang.activeTimer = 20;    // 出現している時間
		fang.isActive = false;
		fang.hasHit = false;
		fangs_.push_back(fang);
	}
	obj_ = Obj3d::Create("sphere"); 
	if (obj_) {
		obj_->SetCamera(camera);
		obj_->SetScale(scale_);
	}
}

void FangEffect::Update(Player *player, Enemy *enemy, Boss *boss, const Vector3 &enemyPos, const Vector3 &bossPos, const LevelData &level) {
	if (isFinished_) {
		return;
	}

	bool allDone = true;

	for (auto &fang : fangs_) {
		if (fang.delayTimer > 0) {
			fang.delayTimer--;
			if (fang.delayTimer <= 0) {
				fang.isActive = true; // 時間が来たら出現
			}
			allDone = false;
		} else if (fang.isActive) {
			fang.activeTimer--;

			// 壁の中なら当たり判定を行わずすぐ消す
			if (Collision::CheckBlockCollision(fang.pos, 0.5f, level)) {
				fang.isActive = false;
				fang.activeTimer = 0;
				continue;
			}

			// 当たり判定
			if (isPlayerCaster_) {
				if (!fang.hasHit && enemy && !enemy->IsDead()) {
					Vector3 diff = { enemyPos.x - fang.pos.x, 0.0f, enemyPos.z - fang.pos.z };
					if (Length(diff) < 1.5f) {
						enemy->TakeDamage(2);
						fang.hasHit = true;
					}
				}
				if (!fang.hasHit && boss && !boss->IsDead()) {
					Vector3 diff = { bossPos.x - fang.pos.x, 0.0f, bossPos.z - fang.pos.z };
					if (Length(diff) < 2.0f) {
						boss->TakeDamage(2);
						fang.hasHit = true;
					}
				}
			}

			if (fang.activeTimer <= 0) {
				fang.isActive = false;
			}
			allDone = false;
		}
	}

	if (allDone) {
		isFinished_ = true;
	}

}

void FangEffect::Draw() {
	if (isFinished_ || !obj_) {
		return;
	}

	for (const auto &fang : fangs_) {
		if (fang.isActive) {
			obj_->SetTranslation(fang.pos);
			obj_->Update();
			obj_->Draw();
		}
	}
}
