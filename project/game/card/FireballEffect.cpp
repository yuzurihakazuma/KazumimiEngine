#include "FireballEffect.h"
#include "Engine/3D/Obj/Obj3d.h"
#include "game/player/Player.h"
#include "game/enemy/Enemy.h"
#include "game/enemy/Boss.h"
#include "engine/math/VectorMath.h"
#include "engine/collision/Collision.h"
#include "game/enemy/EnemyManager.h"
#include "engine/particle/GPUParticleManager.h"
#include <memory>
#include <cmath>

using namespace VectorMath;

void FireballEffect::Start(const Vector3& casterPos, float casterYaw, bool isPlayerCaster, Camera* camera){
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
	if ( obj_ ) {
		obj_->SetCamera(camera);
		obj_->SetScale(scale_);
		obj_->SetTranslation(pos_);
		obj_->Update();
	}
}

void FireballEffect::Update(Player* player, EnemyManager* enemyManager, Boss* boss,
	const Vector3& bossPos, const LevelData& level){

	// 終了済みなら何もしない
	if ( isFinished_ ) {
		return;
	}

	// 弾を進める
	pos_ += velocity_;

	// 炎パーティクルを回転させながら放出
	{
		const float kPI = 3.14159265f;
		rotAngle_ += 0.15f;   // 1フレームあたりの回転速度（大きいほど速く回る）

		// 円周上の3点からパーティクルを出す
		const int kEmitCount = 5;
		const float kRadius = 0.5f; // ボールの周りの半径

		for ( int i = 0; i < kEmitCount; i++ ) {
			float angle = rotAngle_ + ( 2.0f * kPI / kEmitCount ) * i;

			// 円周上の放出位置
			Vector3 emitPos = {
				pos_.x + std::cosf(angle) * kRadius,
				pos_.y,
				pos_.z + std::sinf(angle) * kRadius
			};

			// 接線方向（回転方向）＋上向きの速度
			Vector3 vel = {
		-std::sinf(angle) * 1.5f,
		0.3f + static_cast< float >( rand() % 5 ) * 0.1f,  // 1.5f → 0.3f（ゆっくり上昇）
		std::cosf(angle) * 1.5f
			};

			// 炎の色（オレンジ〜赤）
			Vector4 color = {
				1.0f,                                             // R
				0.2f + static_cast< float >( rand() % 5 ) * 0.1f,   // G（0.2〜0.6）
				0.0f,                                             // B
				1.0f                                              // A
			};

			float lifeTime = 0.5f + static_cast< float >( rand() % 3 ) * 0.1f; // 0.2〜0.4秒
			float scale = 0.5f + static_cast< float >( rand() % 3 ) * 0.05f;  // 0.15〜0.25

			GPUParticleManager::GetInstance()->Emit(emitPos, vel, lifeTime, scale, color);
		}
	}

	// 表示位置を更新
	if ( obj_ ) {
		obj_->SetTranslation(pos_);
		obj_->Update();
	}

	// プレイヤーが使った弾
	if ( isPlayerCaster_ ) {
		if ( enemyManager ) {
			for ( auto& enemy : enemyManager->GetEnemies() ) {
				// 雑魚敵への判定
				if ( enemy && !enemy->IsDead() ) {

					// ループ中の個別の敵の座標を取得する！
					Vector3 ePos = enemy->GetPosition();

					// Y軸を無視してXZ平面で距離判定する
					Vector3 diff = {
						ePos.x - pos_.x,
						0.0f,
						ePos.z - pos_.z
					};

					if ( Length(diff) < 1.8f ) {
						enemy->TakeDamage(1);
						isFinished_ = true;
						return;
					}
				}
			}
		}

		// ボスへの判定
		if ( boss && !boss->IsDead() ) {
			// ボスは大きいので少し広めに取る
			Vector3 diff = {
				bossPos.x - pos_.x,
				0.0f,
				bossPos.z - pos_.z
			};

			if ( Length(diff) < 2.8f ) {
				boss->TakeDamage(1);
				isFinished_ = true;
				return;
			}
		}

		// 遠くまで飛んだら消す
		if ( pos_.x > 100.0f || pos_.x < -100.0f || pos_.z > 100.0f || pos_.z < -100.0f ) {
			isFinished_ = true;
			return;
		}
	}
	// 敵またはボスが使った弾
	else {
		if ( player && !player->IsDead() ) {
			Vector3 playerPos = player->GetPosition();

			// Y軸を無視してXZ平面で距離判定する
			Vector3 diff = {
				playerPos.x - pos_.x,
				0.0f,
				playerPos.z - pos_.z
			};

			if ( Length(diff) < 1.5f ) {
				int damage = 3;

				if ( boss && boss->IsAttackDebuffed() ) {
					damage = 1;
				}

				player->TakeDamage(damage, pos_);
				isFinished_ = true;
				return;
			}
		}
	}

	// 当たり判定の後で壁との衝突を見る
	if ( Collision::CheckBlockCollision(pos_, 0.5f, level) ) {
		isFinished_ = true;
		return;
	}
}

void FireballEffect::Draw(){
	// 有効中だけ描画
	if ( !isFinished_ && obj_ ) {
		obj_->Draw();
	}
}