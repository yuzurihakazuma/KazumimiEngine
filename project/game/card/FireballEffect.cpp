#include "FireballEffect.h"
#include "Engine/3D/Obj/Obj3d.h"
#include "player/Player.h"
#include "enemy/Enemy.h"
#include "enemy/Boss.h"
#include "engine/math/VectorMath.h"
#include "engine/collision/Collision.h"
#include <memory>
#include <cmath>

using namespace VectorMath;

void FireballEffect::Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera) {
	isPlayerCaster_ = isPlayerCaster;
	isFinished_ = false;

	// 前方に進むベクトルを計算
	Vector3 forward = {
		std::sinf(casterYaw),
		0.0f,
		std::cosf(casterYaw)
	};

	// 発生位置を少し前にズラす
	pos_ = {
		casterPos.x + forward.x * 1.5f,
		casterPos.y,
		casterPos.z + forward.z * 1.5f
	};

	// 速度の設定
	velocity_ = {
		forward.x * 0.3f,
		0.0f,
		forward.z * 0.3f
	};

	// 火球用オブジェクト生成
	obj_ = Obj3d::Create("sphere");
	if (obj_) {
		obj_->SetCamera(camera);
		obj_->SetScale(scale_);
		obj_->SetTranslation(pos_);
		obj_->Update();
	}
}

void FireballEffect::Update(Player *player, Enemy *enemy, Boss *boss, const Vector3 &enemyPos, const Vector3 &bossPos, const LevelData &level) {

	// すでに終わっていたら何もしない
	if (isFinished_) {
		return;
	}

	// 座標の更新
	pos_ += velocity_;

	// 壁との当たり判定（※もしCheckBlockCollisionでエラーが出たら教えてください！）
	if (Collision::CheckBlockCollision(pos_, 0.5f, level)) {
		isFinished_ = true; // 壁に当たったら終了！
		return;
	}

	// モデルの更新
	if (obj_) {
		obj_->SetTranslation(pos_);
		obj_->Update();
	}

	// プレイヤーが使った場合の当たり判定
	if (isPlayerCaster_) {

		// 雑魚敵への判定
		if (enemy && !enemy->IsDead()) {
			Vector3 diff = {
				enemyPos.x - pos_.x,
				enemyPos.y - pos_.y,
				enemyPos.z - pos_.z
			};

			if (Length(diff) < 1.5f) {
				enemy->TakeDamage(1);
				isFinished_ = true; // 当たったら終了！
				return;
			}
		}

		// ボスへの判定
		if (boss && !boss->IsDead()) {
			Vector3 diff = {
				bossPos.x - pos_.x,
				bossPos.y - pos_.y,
				bossPos.z - pos_.z
			};

			if (Length(diff) < 2.0f) {
				boss->TakeDamage(1);
				isFinished_ = true; // 当たったら終了！
				return;
			}
		}

		// プレイヤーの開始位置から遠く離れたら消去（※簡略化のため、原点ではなくとりあえず遠くに行ったら消す処理にしています）
		if (pos_.x > 100.0f || pos_.x < -100.0f || pos_.z > 100.0f || pos_.z < -100.0f) {
			isFinished_ = true;
		}

	}
	// 敵が使った場合の当たり判定
	else {
		if (player && !player->IsDead()) {
			Vector3 playerPos = player->GetPosition();
			Vector3 diff = {
				playerPos.x - pos_.x,
				playerPos.y - pos_.y,
				playerPos.z - pos_.z
			};

			if (Length(diff) < 1.5f) {
				int damage = 3;

				// 使用した敵がデバフ状態ならダメージを減らす
				if (enemy && enemy->IsAttackDebuffed()) {
					damage = 1;
				} else if (boss && boss->IsAttackDebuffed()) {
					damage = 1;
				}

				player->TakeDamage(damage, pos_);

				isFinished_ = true; // 当たったら終了！
				return;
			}
		}
	}
}

void FireballEffect::Draw() {

	// 火球描画
	if (!isFinished_ && obj_) {
		obj_->Draw();
	}
}
