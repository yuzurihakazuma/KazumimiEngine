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

void FangEffect::Start(const Vector3& casterPos, float casterYaw, bool isPlayerCaster, Camera* camera) {
	// 使用者情報を保存
	isPlayerCaster_ = isPlayerCaster;
	isFinished_ = false;
	fangs_.clear();

	// 撃った人（敵）の位置を記憶しておく
	casterPos_ = casterPos;

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
	obj_ = Obj3d::Create("fang_sphere");
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
				material->emissive = 1.0f; // 強く光らせる
			}
		}
	}
}

void FangEffect::Update(Player* player, EnemyManager *enemyManager, Boss* boss,
	 const Vector3& bossPos, const LevelData& level) {

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

				for ( int i = 0; i < 30; i++ ) { // 量を15→30に倍増
					Vector3 breakPos = {
						fang.pos.x + ( rand() % 11 - 5 ) * 0.1f, // 発生範囲も少し広げる
						fang.pos.y + static_cast< float >(rand() % 20) * 0.1f,
						fang.pos.z + ( rand() % 11 - 5 ) * 0.1f
					};
					// 上に突き上げる力を強くする
					Vector3 breakVel = {
						( rand() % 11 - 5 ) * 0.05f,
						0.2f + static_cast< float >(rand() % 5) * 0.1f, // 上方向の勢いを爆増
						( rand() % 11 - 5 ) * 0.05f
					};
					Vector4 color = { 0.5f, 0.3f, 0.1f, 1.0f };
					float life = 0.3f + static_cast< float >( rand() % 3 ) * 0.1f;
					float scale = 0.3f + static_cast< float >( rand() % 3 ) * 0.1f; // 破片を少し大きく
					GPUParticleManager::GetInstance()->Emit(breakPos, breakVel, life, scale, color);
				}

				//  【出現時】画面を切り裂く超高速のバチバチ火花！
				int sparkCount = 100; // 量を40→100に激増
				for ( int i = 0; i < sparkCount; i++ ) {
					Vector3 sparkPos = {
						fang.pos.x + ( rand() % 11 - 5 ) * 0.1f,
						fang.pos.y + static_cast< float >(rand() % 20) * 0.1f,
						fang.pos.z + ( rand() % 11 - 5 ) * 0.1f
					};

					// 速度の係数を 0.1f → 0.4f（4倍）にして「弾け飛ぶ」ようにする！
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

				// ★ 事前にプレイヤーの座標を取得しておく
				Vector3 playerPos = { 0,0,0 };
				if (enemyManager) {
					for (auto &enemy : enemyManager->GetEnemies()) {
						// 雑魚敵への判定
						if (!fang.hasHit && enemy && !enemy->IsDead()) {

							// ループ中の個別の敵の座標を取得する！
							Vector3 ePos = enemy->GetPosition();

							Vector3 diff = {
								ePos.x - fang.pos.x,
								0.0f,
								ePos.z - fang.pos.z
							};

							if (Length(diff) < 1.5f) {
								// 1. プレイヤーから敵までの距離を測る
								Vector3 toEnemy = { ePos.x - playerPos.x, 0.0f, ePos.z - playerPos.z };
								float distanceToPlayer = Length(toEnemy);

								// 2. CSVから読み込んだ基本ダメージをセット
								int finalDamage = damage_;

								// 3. 距離によってダメージを足し引きする！
								// （距離の数値やボーナス値は、ゲームのバランスに合わせて自由に変えてください）
								if (distanceToPlayer <= 3.0f) {
									finalDamage += 2;  // 3メートル以内ならダメージ +2 (近接ボーナス)
								} else if (distanceToPlayer >= 8.0f) {
									finalDamage -= 2;  // 8メートル以上離れていたらダメージ -2 (遠距離ペナルティ)
								}

								// 4. 少しのランダムなブレ (-1 ～ +1) も足す
								finalDamage += (rand() % 3) - 1;

								// 5. どんなに下がっても最低1ダメージは与えるようにする
								if (finalDamage < 1) {
									finalDamage = 1;
								}

								// 計算した最終ダメージを与える！
								enemy->TakeDamage(finalDamage);
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
						// --- ボスとの距離を計算 ---
						Vector3 toBoss = { bossPos.x - playerPos.x, 0.0f, bossPos.z - playerPos.z };
						float distanceToPlayer = Length(toBoss);

						int finalDamage = damage_;

						// 距離ボーナス
						if (distanceToPlayer <= 3.0f) {
							finalDamage += 2;
						} else if (distanceToPlayer >= 8.0f) {
							finalDamage -= 2;
						}

						finalDamage += (rand() % 3) - 1;
						if (finalDamage < 1) finalDamage = 1;

						// --- ダメージを与える ---
						boss->TakeDamage(finalDamage);
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
						// 1. 「魔法を撃った敵(casterPos_)」と「プレイヤー」の距離を測る
						Vector3 toCaster = { casterPos_.x - playerPos.x, 0.0f, casterPos_.z - playerPos.z };
						float distanceToCaster = Length(toCaster);

						// 2. CSVから読み込んだ基本ダメージをセット
						int finalDamage = damage_;

						// 3. 距離によってダメージを増減！（敵が近ければ痛い、遠ければ痛くない）
						if (distanceToCaster <= 3.0f) {
							finalDamage += 2;
						} else if (distanceToCaster >= 8.0f) {
							finalDamage -= 2;
						}

						// 4. ランダムなブレ (-1 ～ +1)
						finalDamage += (rand() % 3) - 1;

						// 5. 最低1ダメージは受けるようにする
						if (finalDamage < 1) finalDamage = 1;

						player->TakeDamage(finalDamage, fang.pos);
						fang.hasHit = true;
					}
				}
			}

			// 表示時間が終わったら非アクティブ化
			if (fang.activeTimer <= 0) {
				fang.isActive = false;

				for ( int i = 0; i < 20; i++ ) { // 量を12→20に増加
					Vector3 breakPos = {
						fang.pos.x + ( rand() % 5 - 2 ) * 0.1f,
						fang.pos.y + static_cast< float >(rand() % 20) * 0.1f,
						fang.pos.z + ( rand() % 5 - 2 ) * 0.1f
					};

					// 💡勢い強化：下だけでなく、横にも散らす
					Vector3 breakVel = {
						( rand() % 11 - 5 ) * 0.05f, // 横に広がる勢いを追加
						-0.05f - static_cast< float >(rand() % 5) * 0.05f,
						( rand() % 11 - 5 ) * 0.05f
					};

					Vector4 color = { 0.6f, 0.4f, 0.2f, 1.0f };
					GPUParticleManager::GetInstance()->Emit(breakPos, breakVel, 0.3f, 0.2f, color);
				}

				// 消える瞬間にもショートしたようにバチッと火花を散らす！
				for ( int i = 0; i < 50; i++ ) { // 消滅時専用の火花
					Vector3 sparkPos = {
						fang.pos.x + ( rand() % 11 - 5 ) * 0.1f,
						fang.pos.y + static_cast< float >(rand() % 20) * 0.1f,
						fang.pos.z + ( rand() % 11 - 5 ) * 0.1f
					};
					// 超高速で四方八方に爆散！
					Vector3 sparkVel = {
						( rand() % 11 - 5 ) * 0.3f,
						( rand() % 11 - 5 ) * 0.3f,
						( rand() % 11 - 5 ) * 0.3f
					};

					Vector4 sparkColor;
					if ( rand() % 100 < 50 ) {
						sparkColor = { 1.0f, 1.0f, 1.0f, 1.0f };
					} else {
						sparkColor = { 1.0f, 0.9f, 0.2f, 1.0f };
					}
					// 一瞬で消えるようにする（残像を残さない）
					GPUParticleManager::GetInstance()->Emit(sparkPos, sparkVel, 0.1f, 0.05f, sparkColor);
				}

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