#include "ClawEffect.h"
#include "game/player/Player.h"
#include "game/enemy/EnemyManager.h"
#include "game/enemy/Boss.h"
#include "engine/math/VectorMath.h"
#include "engine/particle/GPUParticleManager.h" // 🌟 パーティクル用に必要
#include <cmath>

using namespace VectorMath;

void ClawEffect::Start(const Vector3& casterPos, float casterYaw, bool isPlayerCaster, Camera* camera){
	isPlayerCaster_ = isPlayerCaster;
	isFinished_ = false;
	timer_ = 0;
	hasHit_ = false;

	// 向きを保存しておく
	casterYaw_ = casterYaw;

	// 目の前に出す
	Vector3 forward = { std::sinf(casterYaw), 0.0f, std::cosf(casterYaw) };
	pos_ = {
			casterPos.x + forward.x * 1.5f,
			casterPos.y + 1.0f, // 高さを少し上げて胴体に当たるようにする
			casterPos.z + forward.z * 1.5f
	};

	obj_ = Obj3d::Create("claw_model");
	if ( obj_ ) {
		obj_->SetCamera(camera);
		obj_->SetScale(scale_);
		obj_->SetTranslation(pos_);

		// モデル自体を鋭く光らせる
		Model* model = obj_->GetModel();
		if ( model && model->GetMaterial() ) {
			Model::Material* mat = model->GetMaterial();
			mat->color = { 0.2f, 0.9f, 1.0f, 1.0f }; // 鋭いシアン
			mat->emissive = 2.0f; // 強く発光させる
		}
		obj_->Update();
	}
}

void ClawEffect::Update(Player* player, EnemyManager* enemyManager, Boss* boss, const Vector3& bossPos, const LevelData& level){

	if ( isFinished_ ) {
		return;
	}

	timer_++;

	// 15フレーム目に「２発目の攻撃」の判定を復活させる
	if ( timer_ == 15 ) {
		hasHit_ = false;
	}

	//  1. 斬撃のコンボアニメーション
	if ( obj_ ) {
		// 1撃目 (1〜14フレーム)：右から左への「横薙ぎ」
		if ( timer_ < 15 ) {
			float progress = static_cast< float >(timer_) / 15.0f;
			float slashYaw = -1.2f + progress * 2.4f; // Y軸で右から左へ

			// Y軸だけ回して横に振る
			obj_->SetRotation({ 0.0f, casterYaw_ + slashYaw, 0.0f });
		}
		// 2撃目 (15〜29フレーム)：下から上への「斬り上げ（アッパー）」
		else {
			float progress = static_cast< float >(timer_ - 15) / 15.0f;

			// X軸(上下)を下向き(+1.5)から上向き(-1.5)へ一気にカチ上げる
			float slashPitch = 1.5f - progress * 3.0f;

			// Z軸(0.5f)で爪を斜めに傾けて、斜め下から斬り上げる「逆袈裟斬り」の形にする
			obj_->SetRotation({ slashPitch, casterYaw_, 0.5f });
		}
		obj_->Update();

		//  2. 軌跡に残像（Trace）を置く
		if ( timer_ % 2 == 0 ) {
			Vector3 tracePos = obj_->GetTranslation();
			Vector3 traceVel = { 0, 0, 0 };
			Vector4 traceColor = { 0.0f, 0.5f, 1.0f, 0.5f }; // 本体より少し暗く透明に
			// その場に少し長め（0.3秒）残すことで残像になる
			GPUParticleManager::GetInstance()->Emit(tracePos, traceVel, 0.3f, 1.5f, traceColor);
		}

		//  3. 先端から散る火花（Sparks）
		for ( int i = 0; i < 3; i++ ) {
			Vector3 sparkVel = {
				( rand() % 11 - 5 ) * 0.4f, // 非常に速い
				( rand() % 11 - 5 ) * 0.4f,
				( rand() % 11 - 5 ) * 0.4f
			};
			Vector4 sparkColor = { 1.0f, 1.0f, 1.0f, 1.0f };
			// 一瞬（0.1秒）で消してバチバチ感を出す
			GPUParticleManager::GetInstance()->Emit(obj_->GetTranslation(), sparkVel, 0.1f, 0.05f, sparkColor);
		}
	}


	//  当たり判定処理（プレイヤー / 敵 それぞれ）
	bool isAttacking = ( timer_ > 1 && timer_ <= 10 ) || ( timer_ >= 15 && timer_ <= 24 );

	if ( isAttacking && !hasHit_ ) {
		if ( isPlayerCaster_ ) {
			// --- 雑魚敵への当たり判定 ---
			if ( enemyManager ) {
				for ( auto& enemy : enemyManager->GetEnemies() ) {
					if ( !enemy || enemy->IsDead() ) continue;

					Vector3 enemyPos = enemy->GetPosition();
					Vector3 diff = { enemyPos.x - pos_.x, 0.0f, enemyPos.z - pos_.z };

					if ( Length(diff) < 2.0f ) {
						int randomDamage = 0;
						if ( timer_ <= 10 ) randomDamage = ( damage_ / 2 ) + ( rand() % 3 ) - 1; // 1撃目は半減
						else randomDamage = damage_ + ( rand() % 3 ) - 1;                    // 2撃目はフル
						if ( randomDamage < 1 ) randomDamage = 1;

						enemy->TakeDamage(randomDamage);
						hasHit_ = true;
						break;
					}
				}
			}

			// --- ボスへの当たり判定 ---
			if ( boss && !boss->IsDead() ) {
				Vector3 diff = { bossPos.x - pos_.x, 0.0f, bossPos.z - pos_.z };
				if ( Length(diff) < 3.0f ) {
					int randomDamage = 0;
					if ( timer_ <= 10 ) randomDamage = ( damage_ / 2 ) + ( rand() % 3 ) - 1;
					else randomDamage = damage_ + ( rand() % 3 ) - 1;
					if ( randomDamage < 1 ) randomDamage = 1;

					boss->TakeDamage(randomDamage);
					hasHit_ = true;
				}
			}
		} else {
			// --- 敵・ボスが使った場合 ---
			if ( player && !player->IsDead() ) {
				Vector3 playerPos = player->GetPosition();
				Vector3 diff = { playerPos.x - pos_.x, 0.0f, playerPos.z - pos_.z };

				if ( Length(diff) < 2.0f ) {
					int randomDamage = 0;
					if ( timer_ <= 10 ) randomDamage = ( damage_ / 2 ) + ( rand() % 3 ) - 1;
					else randomDamage = damage_ + ( rand() % 3 ) - 1;
					if ( randomDamage < 1 ) randomDamage = 1;

					player->TakeDamage(randomDamage, pos_);
					hasHit_ = true;
				}
			}
		}
	}

	// 30フレーム経ったら演出終了
	if ( timer_ >= 30 ) {
		isFinished_ = true;
	}
}

void ClawEffect::Draw(){
	// アニメーションしている間はずっと表示し続けるように修正
	if ( !isFinished_ && obj_ ) {
		obj_->Draw();
	}
}