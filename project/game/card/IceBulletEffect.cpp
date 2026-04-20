#include "IceBulletEffect.h"
#include "game/player/Player.h"
#include "game/enemy/Enemy.h"
#include "game/enemy/Boss.h"
#include "game/enemy/EnemyManager.h"
#include "engine/math/VectorMath.h"
#include "engine/collision/Collision.h"
#include "engine/particle/GPUParticleManager.h" 
#include <cmath>

using namespace VectorMath;

void IceBulletEffect::Start(const Vector3& casterPos, float casterYaw, bool isPlayerCaster, Camera* camera) {
	// 使用者情報を保存
	isPlayerCaster_ = isPlayerCaster;
	isFinished_ = false;

	// 正面方向を計算
	Vector3 forward = { std::sinf(casterYaw), 0.0f, std::cosf(casterYaw) };

	// 使用者の少し前から発射
	pos_ = {
		casterPos.x + forward.x * 1.5f,
		casterPos.y,
		casterPos.z + forward.z * 1.5f
	};

	// 弾速を設定
	velocity_ = {
		forward.x * 0.4f,
		0.0f,
		forward.z * 0.4f
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

void IceBulletEffect::Update(Player* player, EnemyManager *enemyManager, Boss* boss,
	 const Vector3& bossPos, const LevelData& level) {

	// 終了済みなら何もしない
	if (isFinished_) {
		return;
	}

	// 弾を進める
	pos_ += velocity_;

	rotAngle_ += 0.15f;

	// --- 1. コア (Core): 中心で輝く冷気の結晶 ---
	for ( int i = 0; i < 3; i++ ) {
		Vector3 corePos = {
			pos_.x + ( rand() % 11 - 5 ) * 0.05f,
			pos_.y + ( rand() % 11 - 5 ) * 0.05f,
			pos_.z + ( rand() % 11 - 5 ) * 0.05f
		};
		// 外側へ向かう微小な速度
		Vector3 coreVel = {
			( rand() % 11 - 5 ) * 0.02f,
			( rand() % 11 - 5 ) * 0.02f,
			( rand() % 11 - 5 ) * 0.02f
		};
		Vector4 coreColor = { 0.8f, 0.9f, 1.0f, 1.0f }; // 白に近い水色
		float coreLife = 0.1f + static_cast< float >( rand() % 3 ) * 0.05f;
		float coreScale = 0.15f + static_cast< float >( rand() % 3 ) * 0.05f;

		GPUParticleManager::GetInstance()->Emit(corePos, coreVel, coreLife, coreScale, coreColor);
	}

	// --- 2. トレイル (Trail): 通った後に残る冷気のモヤ ---
	// 炎と違い、冷気は「下に沈む」ようにすると氷っぽくなります
	for ( int i = 0; i < 8; i++ ) {
		float angleX = static_cast< float >(rand() % 628) * 0.01f;
		float angleY = static_cast< float >(rand() % 628) * 0.01f;
		float radius = 0.5f;

		Vector3 trailPos = {
			pos_.x + std::cosf(angleX) * std::cosf(angleY) * radius,
			pos_.y + std::sinf(angleY) * radius,
			pos_.z + std::sinf(angleX) * std::cosf(angleY) * radius
		};

		// 進行方向と逆に少し戻りつつ、下に沈む
		Vector3 trailVel = {
			-velocity_.x * 0.5f + ( rand() % 11 - 5 ) * 0.01f,
			-0.05f - static_cast< float >( rand() % 5 ) * 0.01f, // 💡 マイナスにして下へ落とす
			-velocity_.z * 0.5f + ( rand() % 11 - 5 ) * 0.01f
		};

		// 外側に行くほど濃い青に
		float b = 0.8f + static_cast< float >( rand() % 3 ) * 0.1f;
		Vector4 trailColor = { 0.2f, 0.6f, b, 0.6f };

		float trailLife = 0.4f + static_cast< float >( rand() % 5 ) * 0.1f;
		float trailScale = 0.4f + static_cast< float >( rand() % 4 ) * 0.1f;

		GPUParticleManager::GetInstance()->Emit(trailPos, trailVel, trailLife, trailScale, trailColor);
	}

	// --- 3. スパーク (Spark): たまに飛ぶ鋭い氷の破片 ---
	sparkTimer_++;
	if ( sparkTimer_ % 3 == 0 ) {
		for ( int i = 0; i < 3; i++ ) {
			Vector3 sparkVel = {
				( rand() % 11 - 5 ) * 0.15f,
				-0.1f - static_cast< float >(rand() % 5) * 0.05f, // 破片も少し下向きに重力を感じるように
				( rand() % 11 - 5 ) * 0.15f
			};
			Vector4 sparkColor = { 0.5f, 0.9f, 1.0f, 1.0f }; // 鮮やかなシアン
			float sparkLife = 0.2f + static_cast< float >(rand() % 3) * 0.1f;
			float sparkScale = 0.05f + static_cast< float >(rand() % 2) * 0.05f;

			GPUParticleManager::GetInstance()->Emit(pos_, sparkVel, sparkLife, sparkScale, sparkColor);
		}
	}


	// 表示位置を更新
	if ( obj_ ) {
		obj_->SetTranslation(pos_);
		obj_->Update();
	}


	// 表示位置を更新
	if (obj_) {
		obj_->SetTranslation(pos_);
		obj_->Update();
	}

	// プレイヤーが使った場合
	if (isPlayerCaster_) {
		if (enemyManager) {
			for (auto &enemy : enemyManager->GetEnemies()) {
				// 雑魚敵への判定
				if (enemy && !enemy->IsDead()) {

					// ループ中の個別の敵の座標を取得する！
					Vector3 ePos = enemy->GetPosition();

					Vector3 diff = {
						ePos.x - pos_.x,
						0.0f,
						ePos.z - pos_.z
					};

					if (Length(diff) < 1.5f) {
						enemy->TakeDamage(2);
						enemy->Freeze(300);
						isFinished_ = true;
						return;
					}
				}
			}
		}

		// ボスへの判定
		if (boss && !boss->IsDead()) {
			Vector3 diff = {
				bossPos.x - pos_.x,
				0.0f,
				bossPos.z - pos_.z
			};

			if (Length(diff) < 2.5f) {
				boss->TakeDamage(2);
				boss->Freeze(300);
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
	// 敵またはボスが使った場合
	else {
		if (player && !player->IsDead()) {
			Vector3 playerPos = player->GetPosition();

			// Y軸を無視してXZ平面で判定
			Vector3 diff = {
				playerPos.x - pos_.x,
				0.0f,
				playerPos.z - pos_.z
			};

			if (Length(diff) < 1.5f) {
				int damage = 2;

				if (boss && boss->IsAttackDebuffed()) {
					damage = 1;
				}

				player->TakeDamage(damage, pos_);
				isFinished_ = true;
				return;
			}
		}
	}

	// 当たり判定の後で壁との衝突を確認する
	if (Collision::CheckBlockCollision(pos_, 0.5f, level)) {
		isFinished_ = true;
		return;
	}
}

void IceBulletEffect::Draw() {
	//// 有効中だけ描画
	//if (!isFinished_ && obj_) {
	//	obj_->Draw();
	//}
}