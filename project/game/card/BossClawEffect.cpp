#include "BossClawEffect.h"
#include "game/player/Player.h"
#include "engine/math/VectorMath.h"

using namespace VectorMath;

void BossClawEffect::Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera) {
	isFinished_ = false;
	timer_ = 24; // 例：24フレーム（約0.4秒）で攻撃の判定が消える
	hasHit_ = false;

	// ボスの目の前に斬撃を出す
	Vector3 forward = { std::sinf(casterYaw), 0.0f, std::cosf(casterYaw) };
	pos_ = {
		casterPos.x + forward.x * 1.5f,
		casterPos.y + 0.5f,
		casterPos.z + forward.z * 1.5f
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

void BossClawEffect::Update(Player *player, Enemy *enemy, Boss *boss, const Vector3 &enemyPos, const Vector3 &bossPos, const LevelData &level) {
	if (isFinished_) {
		return;
	}

	timer_--;

	// 攻撃がまだ当たっていなければ判定を行う
	if (!hasHit_) {
		// プレイヤーへの当たり判定
		if (player && !player->IsDead()) {
			Vector3 playerPos = player->GetPosition();

			// 高さ(Y)は無視して、XとZの平面上の距離で当たり判定を取る
			Vector3 diff = { playerPos.x - pos_.x, 0.0f, playerPos.z - pos_.z };

			if (Length(diff) < 2.0f) { // 当たり判定の広さ
				// ここが超重要！プレイヤーにダメージと「攻撃元の位置」を渡す
				player->TakeDamage(damage_, pos_);
				hasHit_ = true; // 1回の振りで何度も当たらないようにする
			}
		}
	}

	if (timer_ <= 0) {
		isFinished_ = true;
	}
}

void BossClawEffect::Draw() {
	if (obj_) {
		obj_->Draw();
	}
}


