#include "BossClawEffect.h"
#include "game/player/Player.h"
#include "engine/math/VectorMath.h"
#include "game/enemy/Boss.h"
#include "game/enemy/EnemyManager.h"

using namespace VectorMath;

void BossClawEffect::Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera) {
	isFinished_ = false;
	timer_ = 24; // 例：24フレーム（約0.4秒）で攻撃の判定が消える
	hasHit_ = false;

	float offset = 3.0f;

	// ボスの目の前に斬撃を出す
	Vector3 forward = { std::sinf(casterYaw), 0.0f, std::cosf(casterYaw) };
	pos_ = {
		casterPos.x + forward.x * offset,
		casterPos.y - 1.5f,
		casterPos.z + forward.z * offset
	};

	// プレイヤーのClawと同じモデル（sphere）を生成
	obj_ = Obj3d::Create("sphere");
	if (obj_) {
		obj_->SetCamera(camera);
		obj_->SetScale(scale_);
		obj_->SetTranslation(pos_);
		obj_->Update();
	}
}

void BossClawEffect::Update(Player *player, EnemyManager *enemyManager, Boss *boss,  const Vector3 &bossPos, const LevelData &level) {
	if (isFinished_) {
		return;
	}

	timer_++;

	// 15フレーム目に「２発目の攻撃（縦）」を発生させる
	
	if (timer_ == 15) {
		hasHit_ = false; // 2発目も当たるようにフラグをリセット

		// スケールを縦長に変更
		scale_ = { 0.2f, 1.5f, 1.5f };
		if (obj_) {
			obj_->SetScale(scale_);
			obj_->Update();
		}
	}

	// 当たり判定処理（1〜10フレーム目と、15〜24フレーム目のみ当たり判定を出す）
	bool isAttacking = (timer_ > 1 && timer_ <= 10) || (timer_ >= 15 && timer_ <= 24);

	if (isAttacking && !hasHit_) {
		// ボス専用の魔法なので、プレイヤーへの当たり判定のみ行う
		if (player && !player->IsDead()) {
			Vector3 playerPos = player->GetPosition();
			Vector3 diff = { playerPos.x - pos_.x, 0.0f, playerPos.z - pos_.z };

			if (Length(diff) < 2.0f) { // プレイヤーへの当たり判定の広さ
				int finalDamage = damage_;
				if (boss && boss->IsAttackDebuffed()) {
					finalDamage = finalDamage / 2;
				}

				player->TakeDamage(finalDamage, pos_);
				hasHit_ = true;
			}
		}
	}

	if (timer_ >= 30) {
		isFinished_ = true;
	}
}

void BossClawEffect::Draw() {
	if (obj_) {
		obj_->Draw();
	}
}


