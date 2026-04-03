#include "FangEffect.h"
#include "game/player/Player.h"
#include "game/enemy/Enemy.h"
#include "game/enemy/EnemyManager.h"
#include "game/enemy/Boss.h"
#include "engine/collision/Collision.h"
#include "engine/math/VectorMath.h"
#include <cmath>

using namespace VectorMath;

void FangEffect::Start(const Vector3& casterPos, float casterYaw, bool isPlayerCaster, Camera* camera) {
	// 使用者情報を保存
	isPlayerCaster_ = isPlayerCaster;
	isFinished_ = false;
	fangs_.clear();

	// 正面方向を計算
	Vector3 forward = { std::sinf(casterYaw), 0.0f, std::cosf(casterYaw) };

	// 前方に順番に5本並べる
	for (int i = 0; i < 5; i++) {
		FangData fang;
		fang.pos = {
			casterPos.x + forward.x * (2.0f + i * 2.0f),
			casterPos.y,
			casterPos.z + forward.z * (2.0f + i * 2.0f)
		};

		// 1本ずつ少し遅れて出るようにする
		fang.delayTimer = i * 5;
		fang.activeTimer = 20;
		fang.isActive = false;
		fang.hasHit = false;
		fangs_.push_back(fang);
	}

	// 表示用オブジェクトを生成
	obj_ = Obj3d::Create("sphere");
	if (obj_) {
		obj_->SetCamera(camera);
		obj_->SetScale(scale_);
	}
}

void FangEffect::Update(Player* player, EnemyManager *enemyManager, Boss* boss,
	const Vector3& enemyPos, const Vector3& bossPos, const LevelData& level) {

	// 終了済みなら何もしない
	if (isFinished_) {
		return;
	}

	bool allDone = true;

	for (auto& fang : fangs_) {
		// 出現待機中
		if (fang.delayTimer > 0) {
			fang.delayTimer--;

			// 待機が終わったら有効化
			if (fang.delayTimer <= 0) {
				fang.isActive = true;
			}

			allDone = false;
		}
		// 出現中
		else if (fang.isActive) {
			fang.activeTimer--;

			// 壁の中に出た場合は即終了
			if (Collision::CheckBlockCollision(fang.pos, 0.5f, level)) {
				fang.isActive = false;
				fang.activeTimer = 0;
				continue;
			}

			// プレイヤーが使った場合
			if (isPlayerCaster_) {
				if (enemyManager) {
					for (auto &enemy : enemyManager->GetEnemies()) {
						// 雑魚敵への判定
						if (!fang.hasHit && enemy && !enemy->IsDead()) {
							Vector3 diff = {
								enemyPos.x - fang.pos.x,
								0.0f,
								enemyPos.z - fang.pos.z
							};

							if (Length(diff) < 1.5f) {
								enemy->TakeDamage(2);
								fang.hasHit = true;
							}
						}
					}
				}

				// ボスへの判定
				if (!fang.hasHit && boss && !boss->IsDead()) {
					Vector3 diff = {
						bossPos.x - fang.pos.x,
						0.0f,
						bossPos.z - fang.pos.z
					};

					if (Length(diff) < 2.5f) {
						boss->TakeDamage(2);
						fang.hasHit = true;
					}
				}
			}
			// 敵またはボスが使った場合
			else {
				if (!fang.hasHit && player && !player->IsDead()) {
					Vector3 playerPos = player->GetPosition();

					// Y軸は無視してXZ平面で判定
					Vector3 diff = {
						playerPos.x - fang.pos.x,
						0.0f,
						playerPos.z - fang.pos.z
					};

					if (Length(diff) < 1.5f) {
						int damage = 2;

						if (boss && boss->IsAttackDebuffed()) {
							damage = 1;
						}

						player->TakeDamage(damage, fang.pos);
						fang.hasHit = true;
					}
				}
			}

			// 表示時間が終わったら非アクティブ化
			if (fang.activeTimer <= 0) {
				fang.isActive = false;
			}

			allDone = false;
		}
	}

	// 全てのトゲが終わったら効果終了
	if (allDone) {
		isFinished_ = true;
	}
}

void FangEffect::Draw() {
	// 終了済みまたは描画オブジェクトが無ければ何もしない
	if (isFinished_ || !obj_) {
		return;
	}

	// 有効なトゲだけ描画する
	for (const auto& fang : fangs_) {
		if (fang.isActive) {
			obj_->SetTranslation(fang.pos);
			obj_->Update();
			obj_->Draw();
		}
	}
}