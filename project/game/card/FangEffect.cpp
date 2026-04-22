#include "FangEffect.h"
#include "game/player/Player.h"
#include "game/enemy/Enemy.h"
#include "game/enemy/EnemyManager.h"
#include "game/enemy/Boss.h"
#include "engine/collision/Collision.h"
#include "engine/math/VectorMath.h"
#include "engine/particle/GPUParticleManager.h"
#include <cmath>

using namespace VectorMath;

void FangEffect::Start(const Vector3& casterPos, float casterYaw, bool isPlayerCaster, Camera* camera){
	// 使用者情報を保存
	isPlayerCaster_ = isPlayerCaster;
	isFinished_ = false;
	fangs_.clear();

	// 撃った人（敵）の位置を記憶しておく
	casterPos_ = casterPos;

	// 正面方向を計算
	Vector3 forward = { std::sinf(casterYaw), 0.0f, std::cosf(casterYaw) };

	// 前方に順番に5本並べる
	for ( int i = 0; i < 5; i++ ) {
		FangData fang;
		fang.pos = {
			casterPos.x + forward.x * ( 2.0f + i * 2.0f ),
			casterPos.y,
			casterPos.z + forward.z * ( 2.0f + i * 2.0f )
		};
		// 地面の下深くからスタートさせる
		fang.currentY = casterPos.y - 3.5f;

		// 順番に出る間隔と、地上に留まる時間
		fang.delayTimer = i * 8;
		fang.activeTimer = 40;
		fang.isActive = false;
		fang.hasHit = false;
		fangs_.push_back(fang);
	}

	// 表示用オブジェクトを生成
	obj_ = Obj3d::Create("Fang");
	if ( obj_ ) {
		obj_->SetCamera(camera);
		obj_->SetScale(scale_);

		//  色とテクスチャの設定（岩や土をイメージした色）
		Model* model = obj_->GetModel();
		if ( model ) {
			model->SetTexture("resources/white1x1.png"); // 無地にする
			Model::Material* material = model->GetMaterial();
			if ( material ) {
				material->color = { 0.8f, 0.5f, 0.1f, 0.6f }; // 半透明のオレンジ/茶色
				material->emissive = 2.0f; // Bloom条件を超えるように強く光らせる
			}
		}
	}
}

void FangEffect::Update(Player* player, EnemyManager* enemyManager, Boss* boss,
	const Vector3& bossPos, const LevelData& level){

	// 終了済みなら何もしない
	if ( isFinished_ ) {
		return;
	}

	bool allDone = true;

	for ( auto& fang : fangs_ ) {
		// 出現待機中
		if ( fang.delayTimer > 0 ) {
			fang.delayTimer--;

			// 待機が終わったら有効化
			if ( fang.delayTimer <= 0 ) {
				fang.isActive = true;

				// 【出現時】上に突き上げる岩の破片
				for ( int i = 0; i < 30; i++ ) {
					Vector3 breakPos = {
						fang.pos.x + ( rand() % 11 - 5 ) * 0.1f,
						fang.pos.y + static_cast< float >(rand() % 20) * 0.1f,
						fang.pos.z + ( rand() % 11 - 5 ) * 0.1f
					};
					Vector3 breakVel = {
						( rand() % 11 - 5 ) * 0.02f,
						0.5f + static_cast< float >(rand() % 5) * 0.15f, // 上方向へ爆増
						( rand() % 11 - 5 ) * 0.02f
					};
					Vector4 color = { 0.5f, 0.3f, 0.1f, 1.0f };
					float life = 0.3f + static_cast< float >( rand() % 3 ) * 0.1f;
					float scale = 0.3f + static_cast< float >( rand() % 3 ) * 0.1f;
					GPUParticleManager::GetInstance()->Emit(breakPos, breakVel, life, scale, color);
				}

				// 【出現時】超高速のバチバチ火花
				int sparkCount = 100;
				for ( int i = 0; i < sparkCount; i++ ) {
					Vector3 sparkPos = {
						fang.pos.x + ( rand() % 11 - 5 ) * 0.1f,
						fang.pos.y + static_cast< float >(rand() % 20) * 0.1f,
						fang.pos.z + ( rand() % 11 - 5 ) * 0.1f
					};
					Vector3 sparkVel = {
						( rand() % 11 - 5 ) * 0.4f,
						( rand() % 11 - 5 ) * 0.4f,
						( rand() % 11 - 5 ) * 0.4f
					};
					Vector4 sparkColor;
					if ( rand() % 100 < 50 ) {
						sparkColor = { 1.0f, 1.0f, 1.0f, 1.0f };
					} else {
						sparkColor = { 1.0f, 0.9f, 0.2f, 1.0f };
					}
					float sparkLife = 0.1f + static_cast< float >( rand() % 2 ) * 0.05f;
					float sparkScale = 0.05f + static_cast< float >( rand() % 2 ) * 0.05f;
					GPUParticleManager::GetInstance()->Emit(sparkPos, sparkVel, sparkLife, sparkScale, sparkColor);
				}
			}

			allDone = false;
		}
		// 出現中
		else if ( fang.isActive ) {

			// --- 1. アニメーション（上下移動）処理 ---
			if ( fang.activeTimer > 0 ) {
				fang.activeTimer--;

				// まだ地上に出ていなければ、上に伸びる
				if ( fang.currentY < fang.pos.y ) {
					fang.currentY += 0.15f;
					if ( fang.currentY > fang.pos.y ) {
						fang.currentY = fang.pos.y;
					}
				}
			} else {
				// 時間が来たらゆっくり地面に潜る
				fang.currentY -= 0.15f;
			}

			// 壁の中に出た場合は即終了
			if ( Collision::CheckBlockCollision(fang.pos, 0.5f, level) ) {
				fang.isActive = false;
				fang.activeTimer = 0;
				continue;
			}


			// --- 2. 当たり判定（トゲがちゃんと地上に出ている時だけ判定！） ---
			if ( fang.currentY >= fang.pos.y - 0.5f ) {

				// プレイヤーが使った場合
				if ( isPlayerCaster_ ) {
					Vector3 playerPos = { 0,0,0 };
					if ( enemyManager ) {
						for ( auto& enemy : enemyManager->GetEnemies() ) {
							if ( !fang.hasHit && enemy && !enemy->IsDead() ) {
								Vector3 ePos = enemy->GetPosition();
								Vector3 diff = { ePos.x - fang.pos.x, 0.0f, ePos.z - fang.pos.z };

								if ( Length(diff) < 1.5f ) {
									Vector3 toEnemy = { ePos.x - playerPos.x, 0.0f, ePos.z - playerPos.z };
									float distanceToPlayer = Length(toEnemy);
									int finalDamage = damage_;

									if ( distanceToPlayer <= 3.0f ) {
										finalDamage += 2;
									} else if ( distanceToPlayer >= 8.0f ) {
										finalDamage -= 2;
									}
									finalDamage += ( rand() % 3 ) - 1;
									if ( finalDamage < 1 ) finalDamage = 1;

									enemy->TakeDamage(finalDamage);
									fang.hasHit = true;
								}
							}
						}
					}

					// ボスへの判定
					if ( !fang.hasHit && boss && !boss->IsDead() ) {
						Vector3 diff = { bossPos.x - fang.pos.x, 0.0f, bossPos.z - fang.pos.z };
						if ( Length(diff) < 2.5f ) {
							Vector3 toBoss = { bossPos.x - playerPos.x, 0.0f, bossPos.z - playerPos.z };
							float distanceToPlayer = Length(toBoss);
							int finalDamage = damage_;

							if ( distanceToPlayer <= 3.0f ) {
								finalDamage += 2;
							} else if ( distanceToPlayer >= 8.0f ) {
								finalDamage -= 2;
							}
							finalDamage += ( rand() % 3 ) - 1;
							if ( finalDamage < 1 ) finalDamage = 1;

							boss->TakeDamage(finalDamage);
							fang.hasHit = true;
						}
					}
				}
				// 敵またはボスが使った場合
				else {
					if ( !fang.hasHit && player && !player->IsDead() ) {
						Vector3 playerPos = player->GetPosition();
						Vector3 diff = { playerPos.x - fang.pos.x, 0.0f, playerPos.z - fang.pos.z };

						if ( Length(diff) < 1.5f ) {
							Vector3 toCaster = { casterPos_.x - playerPos.x, 0.0f, casterPos_.z - playerPos.z };
							float distanceToCaster = Length(toCaster);
							int finalDamage = damage_;

							if ( distanceToCaster <= 3.0f ) {
								finalDamage += 2;
							} else if ( distanceToCaster >= 8.0f ) {
								finalDamage -= 2;
							}
							finalDamage += ( rand() % 3 ) - 1;
							if ( finalDamage < 1 ) finalDamage = 1;

							player->TakeDamage(finalDamage, fang.pos);
							fang.hasHit = true;
						}
					}
				}
			} // --- 当たり判定ここまで ---


			// --- 3. 消滅処理（完全に地面の下に潜り切った時） ---
			if ( fang.activeTimer <= 0 && fang.currentY <= fang.pos.y - 3.5f ) {
				fang.isActive = false;

				// 地面に埋まった衝撃で足元から砂が舞う
				for ( int i = 0; i < 20; i++ ) {
					Vector3 breakPos = {
						fang.pos.x + ( rand() % 5 - 2 ) * 0.1f,
						fang.pos.y + static_cast< float >(rand() % 5) * 0.1f, // 足元
						fang.pos.z + ( rand() % 5 - 2 ) * 0.1f
					};
					Vector3 breakVel = {
						( rand() % 11 - 5 ) * 0.05f,
						0.05f + static_cast< float >(rand() % 5) * 0.02f,
						( rand() % 11 - 5 ) * 0.05f
					};
					Vector4 color = { 0.6f, 0.4f, 0.2f, 1.0f };
					GPUParticleManager::GetInstance()->Emit(breakPos, breakVel, 0.3f, 0.2f, color);
				}

				// 残ったエネルギーが足元で弾ける
				for ( int i = 0; i < 30; i++ ) {
					Vector3 sparkPos = {
						fang.pos.x + ( rand() % 11 - 5 ) * 0.1f,
						fang.pos.y + static_cast< float >(rand() % 5) * 0.1f, // 足元
						fang.pos.z + ( rand() % 11 - 5 ) * 0.1f
					};
					Vector3 sparkVel = {
						( rand() % 11 - 5 ) * 0.2f,
						0.1f + static_cast< float >(rand() % 5) * 0.05f,
						( rand() % 11 - 5 ) * 0.2f
					};
					Vector4 sparkColor;
					if ( rand() % 100 < 50 ) {
						sparkColor = { 1.0f, 1.0f, 1.0f, 1.0f };
					} else {
						sparkColor = { 1.0f, 0.9f, 0.2f, 1.0f };
					}
					GPUParticleManager::GetInstance()->Emit(sparkPos, sparkVel, 0.1f, 0.05f, sparkColor);
				}
			}

			allDone = false;
		}
	}

	// 全てのトゲが終わったら効果終了
	if ( allDone ) {
		isFinished_ = true;
	}
}

void FangEffect::Draw(){
	// 終了済みまたは描画オブジェクトが無ければ何もしない
	if ( isFinished_ || !obj_ ) {
		return;
	}

	// 有効なトゲだけ描画する
	for ( const auto& fang : fangs_ ) {
		if ( fang.isActive ) {
			// アニメーション中の currentY を使って描画！
			Vector3 drawPos = { fang.pos.x, fang.currentY, fang.pos.z };
			obj_->SetTranslation(drawPos);
			obj_->Update();
			obj_->Draw();
		}
	}
}