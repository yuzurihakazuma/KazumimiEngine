#include "FistEffect.h"
#include "Engine/3D/Obj/Obj3d.h"
#include "game/player/Player.h"
#include "game/enemy/Enemy.h"
#include "game/enemy/Boss.h"
#include "engine/math/VectorMath.h"
#include <memory>
#include <cmath>

using namespace VectorMath;

void FistEffect::Start(const Vector3& casterPos, float casterYaw, bool isPlayerCaster, Camera* camera) {
	// 使用者情報を保存
	isPlayerCaster_ = isPlayerCaster;
	isFinished_ = false;

	// 効果時間を設定
	punchTimer_ = 10;

	// 正面方向を計算
	Vector3 forward = {
		std::sinf(casterYaw),
		0.0f,
		std::cosf(casterYaw)
	};

	// 使用者の少し前に出す
	pos_ = {
		casterPos.x + forward.x * 1.5f,
		casterPos.y,
		casterPos.z + forward.z * 1.5f
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

void FistEffect::Update(Player* player, Enemy* enemy, Boss* boss,
	const Vector3& enemyPos, const Vector3& bossPos, const LevelData& level) {

	// 終了済みなら何もしない
	if (isFinished_) {
		return;
	}

	// 残り時間を減らす
	punchTimer_--;
	if (punchTimer_ <= 0) {
		isFinished_ = true;
		return;
	}

	// 表示位置を更新
	if (obj_) {
		obj_->SetTranslation(pos_);
		obj_->SetScale(scale_);
		obj_->Update();
	}

	// プレイヤーが使った攻撃
	if (isPlayerCaster_) {

		// 雑魚敵への判定
		if (enemy && !enemy->IsDead()) {
			// Y軸を無視してXZ平面で距離判定する
			Vector3 diff = {
				enemyPos.x - pos_.x,
				0.0f,
				enemyPos.z - pos_.z
			};

			if (Length(diff) < 2.0f) {
				enemy->TakeDamage(1);
				isFinished_ = true;
				return;
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

			if (Length(diff) < 3.0f) {
				boss->TakeDamage(1);
				isFinished_ = true;
				return;
			}
		}
	}
	// 敵またはボスが使った攻撃
	else {
		if (player && !player->IsDead()) {
			Vector3 playerPos = player->GetPosition();

			// Y軸を無視してXZ平面で距離判定する
			Vector3 diff = {
				playerPos.x - pos_.x,
				0.0f,
				playerPos.z - pos_.z
			};

			if (Length(diff) < 2.0f) {
				int damage = 1;

				// 攻撃力低下中ならダメージを下げる
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

	if (isFinished_) {
		return;
	}

	// 有効中だけ描画
	if (!isFinished_ && obj_) {
		obj_->Draw();
	}
}