#include "FireballEffect.h"
#include "Engine/3D/Obj/Obj3d.h"
#include "game/player/Player.h"
#include "game/enemy/Enemy.h"
#include "game/enemy/Boss.h"
#include "engine/math/VectorMath.h"
#include "engine/collision/Collision.h"
#include "game/enemy/EnemyManager.h"
#include <memory>
#include <cmath>

using namespace VectorMath;

void FireballEffect::Start(const Vector3& casterPos, float casterYaw, bool isPlayerCaster, Camera* camera) {
	// 使用者情報を保存
	isPlayerCaster_ = isPlayerCaster;
	isFinished_ = false;

	// 正面方向を計算
	Vector3 forward = {
		std::sinf(casterYaw),
		0.0f,
		std::cosf(casterYaw)
	};

	// 使用者の少し前から発射する
	pos_ = {
		casterPos.x + forward.x * 1.5f,
		casterPos.y,
		casterPos.z + forward.z * 1.5f
	};

	// 発射速度を設定する
	velocity_ = {
		forward.x * 0.3f,
		0.0f,
		forward.z * 0.3f
	};

	// 表示用オブジェクトを生成
	obj_ = Obj3d::Create("sphere");
	if (obj_) {
		obj_->SetCamera(camera);
		obj_->SetScale(scale_);
		obj_->SetTranslation(pos_);
		obj_->Update();
	}
}

void FireballEffect::Update(Player* player, EnemyManager *enemyManager, Boss* boss,
	 const Vector3& bossPos, const LevelData& level) {

	// 終了済みなら何もしない
	if (isFinished_) {
		return;
	}

	// 弾を進める
	pos_ += velocity_;

	// 表示位置を更新
	if (obj_) {
		obj_->SetTranslation(pos_);
		obj_->Update();
	}

	// プレイヤーが使った弾
	if (isPlayerCaster_) {
		if (enemyManager) {
			for (auto &enemy : enemyManager->GetEnemies()) {
				// 雑魚敵への判定
				if (enemy && !enemy->IsDead()) {

					// ループ中の個別の敵の座標を取得する！
					Vector3 ePos = enemy->GetPosition();

					// Y軸を無視してXZ平面で距離判定する
					Vector3 diff = {
						ePos.x - pos_.x,
						0.0f,
						ePos.z - pos_.z
					};

					if (Length(diff) < 1.8f) {
						enemy->TakeDamage(1);
						isFinished_ = true;
						return;
					}
				}
			}
		}

		// ボスへの判定
		if (boss && !boss->IsDead()) {
			// ボスは大きいので少し広めに取る
			Vector3 diff = {
				bossPos.x - pos_.x,
				0.0f,
				bossPos.z - pos_.z
			};

			if (Length(diff) < 2.8f) {
				boss->TakeDamage(1);
				isFinished_ = true;
				return;
			}
		}

		// 遠くまで飛んだら消す
		if (pos_.x > 100.0f || pos_.x < -100.0f || pos_.z > 100.0f || pos_.z < -100.0f) {
			isFinished_ = true;
			return;
		}
	}
	// 敵またはボスが使った弾
	else {
		if (player && !player->IsDead()) {
			Vector3 playerPos = player->GetPosition();

			// Y軸を無視してXZ平面で距離判定する
			Vector3 diff = {
				playerPos.x - pos_.x,
				0.0f,
				playerPos.z - pos_.z
			};

			if (Length(diff) < 1.5f) {
				int damage = 3;

				if (boss && boss->IsAttackDebuffed()) {
					damage = 1;
				}

				player->TakeDamage(damage, pos_);
				isFinished_ = true;
				return;
			}
		}
	}

	// 当たり判定の後で壁との衝突を見る
	if (Collision::CheckBlockCollision(pos_, 0.5f, level)) {
		isFinished_ = true;
		return;
	}
}

void FireballEffect::Draw() {
	// 有効中だけ描画
	if (!isFinished_ && obj_) {
		obj_->Draw();
	}
}