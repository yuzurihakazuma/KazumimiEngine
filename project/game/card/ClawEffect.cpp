#include "ClawEffect.h"
#include "game/player/Player.h"
#include "game/enemy/EnemyManager.h"
#include "game/enemy/Boss.h"
#include "engine/math/VectorMath.h"
#include <cmath>

using namespace VectorMath;

void ClawEffect::Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera) {
	isPlayerCaster_ = isPlayerCaster;
	isFinished_ = false;
	timer_ = 0;
	hasHit_ = false;

	//目の前に斬撃を出す
	Vector3 forward = { std::sinf(casterYaw),0.0f,std::cosf(casterYaw) };
	pos_ = {
			casterPos.x + forward.x * 1.5f,
			casterPos.y + 0.5f,
			casterPos.z + forward.z * 1.5f

	};

	obj_ = Obj3d::Create("sphere");
	if (obj_) {
		obj_->SetCamera(camera);
		obj_->SetScale(scale_);
		obj_->SetTranslation(pos_);
		obj_->Update();
	}


}

void ClawEffect::Update(Player *player, EnemyManager *enemyManager, Boss *boss, const Vector3 &enemyPos, const Vector3 &bossPos, const LevelData &level) {

	if (isFinished_) {
		return;
	}

	timer_++;

	// 15フレーム目に「２発目の攻撃」を発生させる
	if (timer_ == 15) {
		hasHit_ = false;

		scale_ = { 0.2f,1.5f,1.5f };
		if (obj_) {
			obj_->SetScale(scale_);
			obj_->Update();
		}
	}

	// 当たり判定処理
	bool isAttacking = (timer_ > 1 && timer_ <= 10) || (timer_ >= 15 && timer_ <= 24);

	if (isAttacking && !hasHit_) {

		// ==================================================
		// プレイヤーが使った場合
		// ==================================================
		if (isPlayerCaster_) {
			// 🌟 敵への当たり判定（全員分チェック！）
			if (enemyManager) {
				for (auto &enemy : enemyManager->GetEnemies()) {
					if (!enemy || enemy->IsDead()) continue; // 死んでたら無視

					Vector3 enemyPos = enemy->GetPosition();
					Vector3 diff = { enemyPos.x - pos_.x, 0.0f, enemyPos.z - pos_.z };

					if (Length(diff) < 2.0f) { // 当たり判定の広さ
						enemy->TakeDamage(damage_); // 受け取ったダメージ量を与える
						hasHit_ = true; // 1回の振りで何度も当たらないようにする
						break; // 1匹に当たったらこの攻撃の判定は終了（貫通させたいならbreakを消す）
					}
				}
			}

			// ボスへの当たり判定（ここは今のままでOK！）
			if (boss && !boss->IsDead()) {
				Vector3 diff = { bossPos.x - pos_.x, 0.0f, bossPos.z - pos_.z };
				if (Length(diff) < 3.0f) {
					boss->TakeDamage(damage_);
					hasHit_ = true;
				}
			}
		} else {
			// ==================================================
			// 敵・ボスが使った場合
			// ==================================================
			if (player && !player->IsDead()) {
				Vector3 playerPos = player->GetPosition();
				Vector3 diff = { playerPos.x - pos_.x, 0.0f, playerPos.z - pos_.z };

				if (Length(diff) < 2.0f) { // プレイヤーへの当たり判定の広さ
					// プレイヤーのTakeDamageは「ダメージ量」と「攻撃元の座標(pos_)」を渡す！
					player->TakeDamage(damage_, pos_);
					hasHit_ = true;
				}
			}
		
		}
	}
	// 30フレーム経ったら演出終了
	if (timer_ >= 30) {
		isFinished_ = true;
	}

}

void ClawEffect::Draw() {
	// 攻撃判定があるタイミングだけモデルを表示する
	bool isAttacking = (timer_ >= 1 && timer_ <= 10) || (timer_ >= 15 && timer_ <= 24);
	if (!isFinished_ && obj_ && isAttacking) {
		obj_->Draw();
	}
}
