#include "game/card/CardUseSystem.h"

#include <cmath>

#include "Engine/Camera/Camera.h"
#include "Engine/3D/Obj/Obj3d.h"
#include "game/player/Player.h"
#include "game/enemy/Enemy.h"
#include "game/enemy/Boss.h"
#include "engine/math/VectorMath.h"

using namespace VectorMath;

// 初期化
void CardUseSystem::Initialize(Camera* camera) {

	// パンチ用オブジェクト生成
	punchObj_ = Obj3d::Create("sphere");
	if (punchObj_) {
		punchObj_->SetCamera(camera);
		punchObj_->SetScale(punchScale_);
	}

	// 火球用オブジェクト生成
	fireballObj_ = Obj3d::Create("sphere");
	if (fireballObj_) {
		fireballObj_->SetCamera(camera);
		fireballObj_->SetScale(fireballScale_);
	}

	shieldObj_ = Obj3d::Create("sphere");
	if (shieldObj_) {
		shieldObj_->SetCamera(camera);
		shieldObj_->SetScale(shieldScale_);
	}

	iceBulletObj_ = Obj3d::Create("sphere");
	if (iceBulletObj_) {
		iceBulletObj_->SetCamera(camera);
		iceBulletObj_->SetScale(iceBulletScale_);
	}

	fangObj_ = Obj3d::Create("sphere");
	if (fangObj_) {
		fangObj_->SetCamera(camera);
		fangObj_->SetScale({ 0.5f, 2.0f, 0.5f });
	}

	decoyObj_ = Obj3d::Create("sphere");
	if (decoyObj_) {
		decoyObj_->SetCamera(camera);
		decoyObj_->SetScale(decoyScale_);
	}

	Reset();
}

// 更新
void CardUseSystem::Update(Player* player, Enemy* enemy, Boss* boss,
	const Vector3& playerPos, const Vector3& enemyPos, const Vector3& bossPos, const LevelData& level) {

	// 詠唱更新
	if (isCasting_) {
		castTimer_--;

		if (castTimer_ <= 0) {
			isCasting_ = false;
			ExecuteCard(castingCard_, castPos_, castYaw_, isPlayerCasting_, castingPlayer_);
		}
	}

	// パンチ更新
	UpdatePunch(player, enemy, boss, playerPos, enemyPos, bossPos);

	// 火球更新
	UpdateFireball(player, enemy, boss, playerPos, enemyPos, bossPos, level);

	// シールド更新
	UpdateShield(player);

	// 氷の弾の更新
	UpdateIceBullet(player, enemy, boss, playerPos, enemyPos, bossPos, level);

	// 地面からのトゲ攻撃更新
	UpdateFangs(player, enemy, boss, enemyPos, bossPos, level);

	// 身代わり更新
	UpdateDecoy();

	// 攻撃力ダウン更新
	UpdateAtkDown(enemy, boss, enemyPos, bossPos);
}

// 描画
void CardUseSystem::Draw() {

	// パンチ描画
	if (isPunchActive_ && punchObj_) {
		punchObj_->Draw();
	}

	// 火球描画
	if (isFireballActive_ && fireballObj_) {
		fireballObj_->Draw();
	}

	// シールド描画
	if (isShieldVisualActive_ && shieldObj_) {
		shieldObj_->Draw();
	}

	// 氷の弾描画
	if (isIceBulletActive_ && iceBulletObj_) {
		iceBulletObj_->Draw();
	}

	// トゲ描画
	if (isFangsAttackActive_ && fangObj_) {
		for (const auto& fang : fangs_) {
			if (fang.isActive) {
				fangObj_->SetTranslation(fang.pos);
				fangObj_->Update();
				fangObj_->Draw();
			}
		}
	}

	// 身代わり描画
	if (isDecoyActive_ && decoyObj_) {
		decoyObj_->Draw();
	}
}

// カード使用（詠唱開始）
void CardUseSystem::UseCard(const Card& card, const Vector3& casterPos, float casterYaw, bool isPlayerCaster, Player* player) {

	// すでに詠唱中なら新しく使わない
	if (isCasting_) {
		return;
	}

	isCasting_ = true;
	castingCard_ = card;
	castPos_ = casterPos;
	castYaw_ = casterYaw;
	isPlayerCasting_ = isPlayerCaster;
	castingPlayer_ = player;
	castTimer_ = GetCastTime(card);

	// プレイヤーが使った場合のみ詠唱中ロック
	if (isPlayerCaster && player) {
		player->LockAction(castTimer_);
	}
}

// 実際の発動処理
void CardUseSystem::ExecuteCard(const Card& card, const Vector3& casterPos, float casterYaw, bool isPlayerCaster, Player* player) {

	Vector3 forward = {
		std::sinf(casterYaw),
		0.0f,
		std::cosf(casterYaw)
	};

	switch (card.id) {
	case 1: // パンチ
	{
		isPunchActive_ = true;
		isPunchPlayerCaster_ = isPlayerCaster;
		punchTimer_ = 10;

		punchPos_ = {
			casterPos.x + forward.x * 1.5f,
			casterPos.y,
			casterPos.z + forward.z * 1.5f
		};

		if (punchObj_) {
			punchObj_->SetTranslation(punchPos_);
			punchObj_->SetScale(punchScale_);
			punchObj_->Update();
		}
	}
	break;

	case 2: // 火球
	{
		isFireballActive_ = true;
		isFireballPlayerCaster_ = isPlayerCaster;

		fireballPos_ = {
			casterPos.x + forward.x * 1.5f,
			casterPos.y,
			casterPos.z + forward.z * 1.5f
		};

		fireballVelocity_ = {
			forward.x * 0.3f,
			0.0f,
			forward.z * 0.3f
		};

		if (fireballObj_) {
			fireballObj_->SetTranslation(fireballPos_);
			fireballObj_->SetScale(fireballScale_);
			fireballObj_->Update();
		}
	}
	break;

	case 3: // 回復
	{
		if (isPlayerCaster && player != nullptr) {
			player->Heal(5);
		}
	}
	break;

	case 4: // 速度アップ
	{
		if (isPlayerCaster && player != nullptr) {
			float multiplier = static_cast<float>(card.effectValue);
			player->ApplySpeedBuff(multiplier, 300);
		}
	}
	break;

	case 5: // シールド
	{
		if (card.effectType == CardEffectType::Defense) {
			if (isPlayerCaster && player != nullptr) {
				player->ActivateShield(300);
			}
		}
	}
	break;

	case 6: // 氷の弾
	{
		if (!isIceBulletActive_) {
			isIceBulletActive_ = true;
			isIceBulletPLayerCaster_ = isPlayerCaster;
			iceBulletPos_ = casterPos;

			float speed = 0.5f;

			iceBulletVelocity_.x = std::sinf(casterYaw) * speed;
			iceBulletVelocity_.y = 0.0f;
			iceBulletVelocity_.z = std::cosf(casterYaw) * speed;

			if (iceBulletObj_) {
				iceBulletObj_->SetTranslation(iceBulletPos_);
				iceBulletObj_->SetScale(iceBulletScale_);
				iceBulletObj_->Update();
			}
		}
	}
	break;

	case 7: // 地面からトゲ攻撃
	{
		isFangsAttackActive_ = true;
		isFangsPlayerCaster_ = isPlayerCaster;
		fangs_.clear();

		int fangCount = 5;
		float spacing = 2.0f;

		Vector3 dir = { std::sinf(casterYaw), 0.0f, std::cosf(casterYaw) };

		for (int i = 0; i < fangCount; ++i) {
			FangData fang;
			fang.pos = casterPos;
			fang.pos.x += dir.x * spacing * (i + 1);
			fang.pos.z += dir.z * spacing * (i + 1);
			fang.pos.y = 0.0f;

			fang.delayTimer = i * 8;
			fang.activeTimer = 20;
			fang.isActive = false;
			fang.hasHit = false;

			fangs_.push_back(fang);
		}
	}
	break;
	case 8: // 身代わり
	{
		isDecoyActive_ = true;
		decoyPos_ = casterPos; // 使った場所におく
		decoyTimer_ = 300; // 寿命設定
	}
	break;
	case 9: // 攻撃力ダウン（周囲の敵を弱体化）
	{
		isAtkDownActive_ = true;
		isAtkDownPlayerCaster_ = isPlayerCaster;
		atkDownPos_ = casterPos;
		atkDownTimer_ = 20;
	}
	break;
	default:
		break;
	}
}

// カードの詠唱時間
int CardUseSystem::GetCastTime(const Card& card) const {
	return kCommonCastTime_;
}

// リセット
void CardUseSystem::Reset() {

	// 詠唱状態リセット
	isCasting_ = false;
	castingCard_ = { -1, "", 0 };
	castPos_ = { 0.0f, 0.0f, 0.0f };
	castYaw_ = 0.0f;
	isPlayerCasting_ = true;
	castTimer_ = 0;
	castingPlayer_ = nullptr;

	// パンチ状態リセット
	isPunchActive_ = false;
	isPunchPlayerCaster_ = true;
	punchPos_ = { 0.0f, 0.0f, 0.0f };
	punchTimer_ = 0;

	// 火球状態リセット
	isFireballActive_ = false;
	isFireballPlayerCaster_ = true;
	fireballPos_ = { 0.0f, 0.0f, 0.0f };
	fireballVelocity_ = { 0.0f, 0.0f, 0.0f };

	// 氷弾状態リセット
	isIceBulletActive_ = false;
	isIceBulletPLayerCaster_ = true;
	iceBulletPos_ = { 0.0f, 0.0f, 0.0f };
	iceBulletVelocity_ = { 0.0f, 0.0f, 0.0f };

	// シールド見た目リセット
	isShieldVisualActive_ = false;

	// トゲリセット
	isFangsAttackActive_ = false;
	isFangsPlayerCaster_ = true;
	fangs_.clear();
}

void CardUseSystem::CancelCasting() {
	isCasting_ = false;
	castTimer_ = 0;
	castingCard_ = { -1, "", 0 };
	castPos_ = { 0.0f, 0.0f, 0.0f };
	castYaw_ = 0.0f;
	isPlayerCasting_ = true;
	castingPlayer_ = nullptr;
}

// ブロックとの衝突判定
bool CardUseSystem::CheckBlockCollision(const Vector3& pos, float radius, const LevelData& level) {

	AABB projectileAABB;
	projectileAABB.min = { pos.x - radius, pos.y - radius, pos.z - radius };
	projectileAABB.max = { pos.x + radius, pos.y + radius, pos.z + radius };

	for (int z = 0; z < level.height; z++) {
		for (int x = 0; x < level.width; x++) {

			if (level.tiles[z][x] != 1) {
				continue;
			}

			float worldX = x * level.tileSize;
			float worldZ = z * level.tileSize;

			AABB blockAABB;
			blockAABB.min = { worldX - 1.0f, level.baseY,        worldZ - 1.0f };
			blockAABB.max = { worldX + 1.0f, level.baseY + 2.0f, worldZ + 1.0f };

			if (Collision::IsCollision(projectileAABB, blockAABB)) {
				return true;
			}
		}
	}

	return false;
}

void CardUseSystem::UpdateShield(Player* player) {
	if (!player || !shieldObj_) {
		return;
	}

	isShieldVisualActive_ = player->IsShieldActive();

	if (isShieldVisualActive_) {
		Vector3 shieldPos = player->GetPosition();
		shieldObj_->SetTranslation(shieldPos);
		shieldObj_->Update();
	}
}

void CardUseSystem::UpdatePunch(Player* player, Enemy* enemy, Boss* boss,
	const Vector3& playerPos, const Vector3& enemyPos, const Vector3& bossPos) {

	if (!isPunchActive_) {
		return;
	}

	punchTimer_--;

	if (punchTimer_ <= 0) {
		isPunchActive_ = false;
		return;
	}

	if (punchObj_) {
		punchObj_->SetTranslation(punchPos_);
		punchObj_->SetScale(punchScale_);
		punchObj_->Update();
	}

	if (isPunchPlayerCaster_) {

		if (enemy && !enemy->IsDead()) {
			Vector3 diff = {
				enemyPos.x - punchPos_.x,
				enemyPos.y - punchPos_.y,
				enemyPos.z - punchPos_.z
			};

			if (Length(diff) < 2.0f) {
				// ここでダメージを計算する！
				int damage = 1; // パンチの本来のダメージ

				// もし使用した敵がデバフ状態ならダメージを減らす（例：0にする）
				if (enemy && enemy->IsAttackDebuffed()) {
					damage = 0; // 弱体化中のダメージ！
				} else if (boss && boss->IsAttackDebuffed()) {
					damage = 0; // ボスが弱体化中のダメージ！
				}

				// 第2引数でノックバックの方向(パンチの座標)を渡す
				player->TakeDamage(damage, punchPos_);

				isPunchActive_ = false; // 当たったらパンチの判定を消す
				return;
			
			}
		}

		if (boss && !boss->IsDead()) {
			Vector3 diff = {
				bossPos.x - punchPos_.x,
				bossPos.y - punchPos_.y,
				bossPos.z - punchPos_.z
			};

			if (Length(diff) < 2.5f) {
				boss->TakeDamage(1);
				isPunchActive_ = false;
				return;
			}
		}
	} else {
		if (player && !player->IsDead()) {
			Vector3 diff = {
				playerPos.x - punchPos_.x,
				playerPos.y - punchPos_.y,
				playerPos.z - punchPos_.z
			};

			if (Length(diff) < 2.0f) {
				player->TakeDamage(1, punchPos_);
				isPunchActive_ = false;
				return;
			}
		}
	}
}

void CardUseSystem::UpdateFireball(Player* player, Enemy* enemy, Boss* boss,
	const Vector3& playerPos, const Vector3& enemyPos, const Vector3& bossPos,
	const LevelData& level) {

	if (!isFireballActive_) {
		return;
	}

	fireballPos_ += fireballVelocity_;

	if (CheckBlockCollision(fireballPos_, 0.5f, level)) {
		isFireballActive_ = false;
		return;
	}

	if (fireballObj_) {
		fireballObj_->SetTranslation(fireballPos_);
		fireballObj_->SetScale(fireballScale_);
		fireballObj_->Update();
	}

	if (isFireballPlayerCaster_) {

		if (enemy && !enemy->IsDead()) {
			Vector3 diff = {
				enemyPos.x - fireballPos_.x,
				enemyPos.y - fireballPos_.y,
				enemyPos.z - fireballPos_.z
			};

			if (Length(diff) < 1.5f) {
				int damage = 3;

				// もし使用した敵がデバフ状態ならダメージを減らす（例：半分にする、1にする等）
				if (enemy && enemy->IsAttackDebuffed()) {
					damage = 1; // 弱体化中のダメージ！
				} else if (boss && boss->IsAttackDebuffed()) {
					damage = 1; // ボスが弱体化中のダメージ！
				}

				player->TakeDamage(damage,fireballPos_); // 計算したダメージを与える
				// ----------------------------------------------------

				// 当たったら火球を消す
				isFireballActive_ = false;
				return;
			}
		}

		if (boss && !boss->IsDead()) {
			Vector3 diff = {
				bossPos.x - fireballPos_.x,
				bossPos.y - fireballPos_.y,
				bossPos.z - fireballPos_.z
			};

			if (Length(diff) < 2.0f) {
				boss->TakeDamage(1);
				isFireballActive_ = false;
				return;
			}
		}

		if (Length(fireballPos_ - playerPos) > 20.0f) {
			isFireballActive_ = false;
		}
	} else {
		if (player && !player->IsDead()) {
			Vector3 diff = {
				playerPos.x - fireballPos_.x,
				playerPos.y - fireballPos_.y,
				playerPos.z - fireballPos_.z
			};

			if (Length(diff) < 1.5f) {
				player->TakeDamage(1, fireballPos_);
				isFireballActive_ = false;
				return;
			}
		}

		Vector3 originPos = enemyPos;
		if (boss && !isFireballPlayerCaster_) {
			originPos = bossPos;
		}

		if (Length(fireballPos_ - originPos) > 20.0f) {
			isFireballActive_ = false;
		}
	}
}

void CardUseSystem::UpdateIceBullet(Player* player, Enemy* enemy, Boss* boss,
	const Vector3& playerPos, const Vector3& enemyPos, const Vector3& bossPos,
	const LevelData& level) {

	if (!isIceBulletActive_) {
		return;
	}

	// 弾の移動
	iceBulletPos_ += iceBulletVelocity_;

	// ブロック衝突
	if (CheckBlockCollision(iceBulletPos_, 0.5f, level)) {
		isIceBulletActive_ = false;
		return;
	}

	// 描画更新
	if (iceBulletObj_) {
		iceBulletObj_->SetTranslation(iceBulletPos_);
		iceBulletObj_->SetScale(iceBulletScale_);
		iceBulletObj_->Update();
	}

	// =========================
	// プレイヤーが撃った氷弾
	// =========================
	if (isIceBulletPLayerCaster_) {

		// Enemy判定
		if (enemy && !enemy->IsDead()) {
			Vector3 diff = {
				enemyPos.x - iceBulletPos_.x,
				enemyPos.y - iceBulletPos_.y,
				enemyPos.z - iceBulletPos_.z
			};

			if (Length(diff) < 1.5f) {
				enemy->TakeDamage(2);
				enemy->Freeze(300);
				isIceBulletActive_ = false;
				return;
			}
		}

		// Boss判定
		if (boss && !boss->IsDead()) {
			Vector3 diff = {
				bossPos.x - iceBulletPos_.x,
				bossPos.y - iceBulletPos_.y,
				bossPos.z - iceBulletPos_.z
			};

			if (Length(diff) < 2.0f) {
				boss->TakeDamage(2);
				isIceBulletActive_ = false;
				return;
			}
		}

		// 飛距離制限
		if (Length(iceBulletPos_ - playerPos) > 20.0f) {
			isIceBulletActive_ = false;
		}
	}

	// =========================
	// 敵・ボスが撃った氷弾
	// =========================
	else {

		if (player && !player->IsDead()) {
			Vector3 diff = {
				playerPos.x - iceBulletPos_.x,
				playerPos.y - iceBulletPos_.y,
				playerPos.z - iceBulletPos_.z
			};

			if (Length(diff) < 1.5f) {
				// ここでダメージを計算する！
				int damage = 2; 

				// もし使用した敵がデバフ状態ならダメージを減らす（例：0にする）
				if (enemy && enemy->IsAttackDebuffed()) {
					damage = 0; // 弱体化中のダメージ！
				} else if (boss && boss->IsAttackDebuffed()) {
					damage = 0; // ボスが弱体化中のダメージ！
				}

				// アイスボールの方向を渡す
				player->TakeDamage(damage, iceBulletPos_);

				isIceBulletActive_ = false;
				return;
			}
		}

		Vector3 originPos = enemyPos;

		// ボスが撃った場合はボス位置を基準
		if (boss) {
			originPos = bossPos;
		}

		// 飛距離制限
		if (Length(iceBulletPos_ - originPos) > 20.0f) {
			isIceBulletActive_ = false;
		}
	}
}


void CardUseSystem::UpdateFangs(Player *player, Enemy *enemy, Boss *boss, const Vector3 &enemyPos, const Vector3 &bossPos, const LevelData &level) {

	if (!isFangsAttackActive_) {
		return;
	}

	bool allDone = true;

	for (auto &fang : fangs_) {
		// すでに寿命が尽きたトゲは完全に無視する
		if (fang.activeTimer <= 0) {
			continue;
		}

		allDone = false; // まだ出番が残っているトゲがある

		// 待機時間があるなら減らす
		if (fang.delayTimer > 0) {
			fang.delayTimer--;
		} else {
			// 待機時間がゼロになったら地面から出す

			//　壁の中かどうかチェックする
			if (CheckBlockCollision(fang.pos, 0.5f, level)) {
				fang.activeTimer = 0; // 寿命をセロにして
				fang.isActive = false; // 非アクティブする
				continue; // 壁の中には出さずに次のトゲへ
			}

			// 壁じゃなければトゲを出す
			fang.isActive = true;
			fang.activeTimer--; // 毎フレーム寿命を減らす

			// 当たり判定 (プレイヤーが使った場合)
			if (isFangsPlayerCaster_ && !fang.hasHit) {

				// 雑魚敵との当たり判定
				if (enemy && !enemy->IsDead()) {
					Vector3 diffEnemy = {
						enemyPos.x - fang.pos.x,
						0.0f,
						enemyPos.z - fang.pos.z
					};

					if (Length(diffEnemy) < 1.2f) { //　雑魚敵の当たり判定の広さ
						enemy->TakeDamage(2); // csvのダメージ
						fang.hasHit = true; // 1回のトゲにつき１回だけ当たる
					}
				}

				// ボスとの当たり判定
				if (boss && !boss->IsDead() && !fang.hasHit) {
					Vector3 diffBoss = {
						bossPos.x - fang.pos.x,
						0.0f,
						bossPos.z - fang.pos.z
					};

					if (Length(diffBoss) < 2.0f) { // ボスは体が大きいので当たり判定を広めにする
						boss->TakeDamage(2); // ボスにダメージ
						fang.hasHit = true;
					}
				}
			}

			// 寿命がゼロになったら引っ込める
			if (fang.activeTimer <= 0) {
				fang.isActive = false;
			}
		}
	}

	// 全ての戸部の寿命がゼロになったら演出終了
	if (allDone) {
		isFangsAttackActive_ = false;
	}
}

void CardUseSystem::UpdateDecoy() {

	if (!isDecoyActive_) {
		return;
	}

	// タイマーを減らす
	decoyTimer_--;

	// 時間切れで消滅
	if (decoyTimer_ <= 0) {
		isDecoyActive_ = false;
		return;
	}

	// モデルの更新
	if (decoyObj_) {
		decoyObj_->SetTranslation(decoyPos_);
		decoyObj_->SetScale(decoyScale_);
		decoyObj_->Update();
	}

}

void CardUseSystem::UpdateAtkDown(Enemy *enemy, Boss *boss, const Vector3 &enemyPos, const Vector3 &bossPos) {
	if (!isAtkDownActive_) return;

	atkDownTimer_--;
	if (atkDownTimer_ <= 0) {
		isAtkDownActive_ = false;
		return;
	}

	if (isAtkDownPlayerCaster_) {
		// 雑魚敵への判定
		if (enemy && !enemy->IsDead()) {
			Vector3 diff = { enemyPos.x - atkDownPos_.x, 0.0f, enemyPos.z - atkDownPos_.z };
			if (Length(diff) < 5.0f) {
				enemy->ApplyAttackDebuff(300); // 300フレーム(約5秒) デバフ状態にする！
			}
		}

		// ボスへの判定
		if (boss && !boss->IsDead()) {
			Vector3 diffBoss = { bossPos.x - atkDownPos_.x, 0.0f, bossPos.z - atkDownPos_.z };
			if (Length(diffBoss) < 8.0f) {
				boss->ApplyAttackDebuff(300);
			}
		}
	}
}
