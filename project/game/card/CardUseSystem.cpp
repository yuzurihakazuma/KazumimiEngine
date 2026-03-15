#include "game/card/CardUseSystem.h"

#include <cmath>

#include "Engine/Camera/Camera.h"
#include "Engine/3D/Obj/Obj3d.h"
#include "game/player/Player.h"
#include "game/enemy/Enemy.h"
#include "engine/math/VectorMath.h"

using namespace VectorMath;

// 初期化
void CardUseSystem::Initialize(Camera *camera) {

	// パンチ用オブジェクト生成
	punchObj_ = Obj3d::Create("sphere");
	if (punchObj_) {
		punchObj_->SetCamera(camera);          // カメラ設定
		punchObj_->SetScale(punchScale_);      // 初期サイズ設定
	}

	// 火球用オブジェクト生成
	fireballObj_ = Obj3d::Create("sphere");
	if (fireballObj_) {
		fireballObj_->SetCamera(camera);       // カメラ設定
		fireballObj_->SetScale(fireballScale_);// 初期サイズ設定
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
		fangObj_->SetScale({ 0.5f,2.0f,0.5f });
	}

	// 演出状態をリセット
	Reset();
}

// 更新
void CardUseSystem::Update(Player *player, Enemy *enemy, const Vector3 &playerPos, const Vector3 &enemyPos, const LevelData &level) {

	// パンチ更新
	UpdatePunch(player, enemy, playerPos, enemyPos);

	// 火球更新
	UpdateFireball(player, enemy, playerPos, enemyPos, level);

	//シールドの見た目更新
	UpdateShield(player);

	// 氷の弾の更新
	UpdateIceBullet(player, enemy, playerPos, enemyPos, level);

	UpdateFangs(player, enemy, enemyPos,level);
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

	// シールドが有効な時だけ描画
	if (isShieldVisualActive_ && shieldObj_) {
		shieldObj_->Draw();
	}

	//　氷の描画
	if (isIceBulletActive_ && iceBulletObj_) {
		iceBulletObj_->Draw();
	}

	// 地面からのトゲ攻撃の描画
	if (isFangsAttackActive_ && fangObj_) {
		//全てのトゲの位置をチェック
		for (const auto &fang : fangs_) {
			// 今,地面に出ているトゲだけ描画
			if (fang.isActive) {
				fangObj_->SetTranslation(fang.pos); // 位置をセット
				fangObj_->Update();                 // 行列を計算
				fangObj_->Draw();                   // 描画
			}
		}
	}
}

// カード使用
void CardUseSystem::UseCard(const Card &card, const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Player *player) {

	// 使用者の前方向を計算
	Vector3 forward = {
		std::sinf(casterYaw),
		0.0f,
		std::cosf(casterYaw)
	};

	switch (card.id) {
	case 1: // パンチ
	{
		// プレイヤーが使った場合のみ行動ロック
		if (isPlayerCaster && player) {
			player->LockAction(10); // パンチ中は少し硬直
		}

		isPunchActive_ = true;                  // パンチ演出開始
		isPunchPlayerCaster_ = isPlayerCaster;  // 使用者情報を保存
		punchTimer_ = 10;                       // 表示時間設定

		// 使用者前方にパンチを配置
		punchPos_ = {
			casterPos.x + forward.x * 1.5f,
			casterPos.y,
			casterPos.z + forward.z * 1.5f
		};

		if (punchObj_) {
			punchObj_->SetTranslation(punchPos_); // パンチ位置更新
			punchObj_->SetScale(punchScale_);     // パンチサイズ更新
			punchObj_->Update();                  // 行列更新
		}
	}
	break;

	case 2: // 火球
	{
		// プレイヤーが使った場合のみ行動ロック
		if (isPlayerCaster && player) {
			player->LockAction(20); // 火球は少し長めに硬直
		}

		isFireballActive_ = true;                    // 火球演出開始
		isFireballPlayerCaster_ = isPlayerCaster;    // 使用者情報を保存

		// 使用者前方に火球を配置
		fireballPos_ = {
			casterPos.x + forward.x * 1.5f,
			casterPos.y,
			casterPos.z + forward.z * 1.5f
		};

		// 前方向へ飛ばす速度
		fireballVelocity_ = {
			forward.x * 0.3f,
			0.0f,
			forward.z * 0.3f
		};

		if (fireballObj_) {
			fireballObj_->SetTranslation(fireballPos_); // 火球位置更新
			fireballObj_->SetScale(fireballScale_);     // 火球サイズ更新
			fireballObj_->Update();                     // 行列更新
		}
	}
	break;

	case 3: //回復
	{
		//　プレイヤーが使った場合のみ効果を発揮
		if (isPlayerCaster && player != nullptr) {
			player->Heal(5);
		}
	}
	break;
	case 4: //速度アップ
	{

		//プレイヤーが使った場合のみ効果を発揮する
		if (isPlayerCaster && player != nullptr) {
			//csvで設定した　effctValueをfloatに変換
			float multiplier = static_cast<float>(card.effectValue);
			//5秒間移動側を上げる
			player->ApplySpeedBuff(multiplier, 300);
		}


	}
	break;
	case 5: //シールド
	{
		// 防御（シールド）カードの処理を追加
		if (card.effectType == CardEffectType::Defense) {
			if (isPlayerCaster && player!=nullptr) {
				// 60FPS想定で 5秒 × 60フレーム = 300フレーム
				player->ActivateShield(300);
			}
		}
	}
	case 6: //氷の弾
	{
		if (!isIceBulletActive_) {
			isIceBulletActive_ = true;
			isIceBulletPLayerCaster_ = isPlayerCaster;
			iceBulletPos_ = casterPos;

			//向いている方向から飛んでいく速度を計算
			float speed = 0.5f;

			iceBulletVelocity_.x = std::sinf(casterYaw) * speed;
			iceBulletVelocity_.y = 0.0f; // 上下には飛ばさない
			iceBulletVelocity_.z = std::cosf(casterYaw) * speed;

			if (player) {
				player->LockAction(20); // プレイヤーが撃った時のスキ
			}

		}
	}
	break;

	case 7: // 地面からトゲ攻撃
	{
		isFangsAttackActive_ = true;
		isFangsPlayerCaster_ = isPlayerCaster;
		fangs_.clear();

		int fangCount = 5;    // トゲを出す数
		float spacing = 2.0f; // 度下同士の間隔

		// 向いている方向
		Vector3 dir = {std::sinf(casterYaw),0.0f,std::cosf(casterYaw)};

		for (int i = 0; i < fangCount; ++i) {
			FangData fang;
			fang.pos = casterPos;
			fang.pos.x += dir.x * spacing * (i + 1);
			fang.pos.z += dir.z * spacing * (i + 1);
			fang.pos.y = 0.0f; // 足元から出す

			fang.delayTimer = i * 8; // 前のトゲより8フレーム遅らせて出す
			fang.activeTimer = 20; // 出現時間
			fang.isActive = false;
			fang.hasHit = false;

			fangs_.push_back(fang);
		}

		if (player) {
			player->LockAction(30);
		}
	}
	break;

	default:
		// 未対応カードは何もしない
		break;
	}
}

// リセット
void CardUseSystem::Reset() {

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
}

// ブロックとの衝突判定
bool CardUseSystem::CheckBlockCollision(const Vector3 &pos, float radius, const LevelData &level) {

	// 火球用のAABBを作成
	AABB projectileAABB;
	projectileAABB.min = { pos.x - radius, pos.y - radius, pos.z - radius };
	projectileAABB.max = { pos.x + radius, pos.y + radius, pos.z + radius };

	// タイルを走査
	for (int z = 0; z < level.height; z++) {
		for (int x = 0; x < level.width; x++) {

			// ブロック以外は無視
			if (level.tiles[z][x] != 1) {
				continue;
			}

			float worldX = x * level.tileSize;
			float worldZ = z * level.tileSize;

			// ブロックのAABBを作成
			AABB blockAABB;
			blockAABB.min = { worldX - 1.0f, level.baseY,         worldZ - 1.0f };
			blockAABB.max = { worldX + 1.0f, level.baseY + 2.0f,  worldZ + 1.0f };

			// 火球とブロックが当たったらtrue
			if (Collision::IsCollision(projectileAABB, blockAABB)) {
				return true;
			}
		}
	}

	return false; // どのブロックにも当たっていない
}

void CardUseSystem::UpdateShield(Player *player) {
	if (!player || !shieldObj_) {
		return;
	}

	//プレイヤーのシールド状態と見た目の表示フラグを同期
	isShieldVisualActive_ = player->IsShieldActive();

	if (isShieldVisualActive_) {
		//プレイヤーの位置にシールドを配置
		Vector3 shieldPos = player->GetPosition();
		shieldObj_->SetTranslation(shieldPos);
		shieldObj_->Update();


	}
}

void CardUseSystem::UpdateIceBullet(Player *player, Enemy *enemy, const Vector3 &playerPos, const Vector3 &enemyPos, const LevelData &level) {
	if (!isIceBulletActive_) {
		return;
	}

	//弾を移動させる
	iceBulletPos_ += iceBulletVelocity_;

	//ブロックに当たったら消滅
	if (CheckBlockCollision(iceBulletPos_, 0.5f, level)) {
		isIceBulletActive_ = false;
		return;
	}

	//見た目の位置を更新
	if (iceBulletObj_){
		iceBulletObj_->SetTranslation(iceBulletPos_);
		iceBulletObj_->SetScale(iceBulletScale_);
		iceBulletObj_->Update();
	}

	//プレイヤーが撃った弾の場合
	if (isIceBulletPLayerCaster_) {
		//敵との当たり判定
		if (enemy && !enemy->IsDead()) {
			Vector3 diff = {
				enemyPos.x - iceBulletPos_.x,
				enemyPos.y - iceBulletPos_.y,
				enemyPos.z - iceBulletPos_.z
			};

			//敵に当たった
			if (Length(diff) < 1.5f) {
				enemy->TakeDamage(2);

				//敵を5秒間凍結させる
				enemy->Freeze(300);

				isIceBulletActive_ = false; //当たった消滅
				return;
			}
		}

		//一定距離飛んだら消滅
		if (Length(iceBulletPos_ - playerPos) > 20.0f) {
			isIceBulletActive_ = false;
		}
	}
}

void CardUseSystem::UpdateFangs(Player *player, Enemy *enemy, const Vector3 &enemyPos, const LevelData &level) {

	if (!isFangsAttackActive_) {
		return;
	}

	bool allDone = true; // 全てのトゲが終わったらチェック

	for (auto &fang : fangs_) {
		if (fang.activeTimer <= 0) {
			continue;  // すでに終わっているトゲは無視
		}

		allDone = false; // すでに引っ込んだトゲは無視

		// 待機時間があるなら減らす
		if (fang.delayTimer > 0) {
			fang.delayTimer--;
		} else {

			// トゲが出る座標がブロック(壁)と被っていたら
			if (CheckBlockCollision(fang.pos, 0.5f, level)) {
				fang.activeTimer = 0;  // 寿命をゼロにして
				fang.isActive = false; // 非アクティブにする
				continue;              // 壁の中には出さずに次のトゲへ！
			}

			// 壁じゃなければトゲを出す
			fang.isActive = true;
			fang.activeTimer--;

			// 当たり判定
			if (isFangsPlayerCaster_ && enemy && !enemy->IsDead() && !fang.hasHit) {
				// 高さは無視して,XとZの距離だけで判定する
				Vector3 diff = { enemyPos.x - fang.pos.x,0.0f,enemyPos.z - fang.pos.z };

				if (Length(diff) < 1.2f) { // 当たり判定の広さ
					enemy->TakeDamage(2); // csvのダメージを与える
					fang.hasHit = true;   // 1つのトゲにつき１回だけ当たるようにする
				}
			}

			//時間切れで引っ込む
			if (fang.activeTimer <= 0) {
				fang.isActive = false;
			}

			// すべてのトゲの寿命がゼロ(allDone == true)になったら演出終了
			if (allDone) {
				isFangsAttackActive_ = false;
			}
		}
	}

	//全部終わったら演出終了
	if (allDone) {
		isFangsAttackActive_ = false;
	}
}

// パンチ更新
void CardUseSystem::UpdatePunch(Player *player, Enemy *enemy, const Vector3 &playerPos, const Vector3 &enemyPos) {

	if (!isPunchActive_) {
		return; // パンチ演出中でなければ何もしない
	}

	// 残り時間更新
	punchTimer_--;

	if (punchTimer_ <= 0) {
		isPunchActive_ = false; // 時間切れで終了
		return;
	}

	// パンチオブジェクト更新
	if (punchObj_) {
		punchObj_->SetTranslation(punchPos_);
		punchObj_->SetScale(punchScale_);
		punchObj_->Update();
	}

	// プレイヤーが使ったパンチ
	if (isPunchPlayerCaster_) {

		if (enemy && !enemy->IsDead()) {
			Vector3 diff = {
				enemyPos.x - punchPos_.x,
				enemyPos.y - punchPos_.y,
				enemyPos.z - punchPos_.z
			}; // 敵との距離

			if (Length(diff) < 2.0f) {
				enemy->TakeDamage(1);   // 敵にダメージ
				isPunchActive_ = false; // ヒットしたら終了
			}
		}
	}
	// 敵が使ったパンチ
	else {

		if (player && !player->IsDead()) {
			Vector3 diff = {
				playerPos.x - punchPos_.x,
				playerPos.y - punchPos_.y,
				playerPos.z - punchPos_.z
			}; // プレイヤーとの距離

			if (Length(diff) < 2.0f) {
				player->TakeDamage(1, punchPos_); // プレイヤーにダメージ
				isPunchActive_ = false;           // ヒットしたら終了
			}
		}
	}
}

// 火球更新
void CardUseSystem::UpdateFireball(Player *player, Enemy *enemy, const Vector3 &playerPos, const Vector3 &enemyPos, const LevelData &level) {

	if (!isFireballActive_) {
		return; // 火球演出中でなければ何もしない
	}

	// 火球移動
	fireballPos_ += fireballVelocity_;

	// ブロックに当たったら消す
	if (CheckBlockCollision(fireballPos_, 0.5f, level)) {
		isFireballActive_ = false; // 壁ヒットで火球消滅
		return;
	}

	// 火球オブジェクト更新
	if (fireballObj_) {
		fireballObj_->SetTranslation(fireballPos_);
		fireballObj_->SetScale(fireballScale_);
		fireballObj_->Update();
	}

	// プレイヤーが使った火球
	if (isFireballPlayerCaster_) {

		if (enemy && !enemy->IsDead()) {
			Vector3 diff = {
				enemyPos.x - fireballPos_.x,
				enemyPos.y - fireballPos_.y,
				enemyPos.z - fireballPos_.z
			}; // 敵との距離

			if (Length(diff) < 1.5f) {
				enemy->TakeDamage(1);      // 敵にダメージ
				isFireballActive_ = false; // ヒットしたら終了
				return;
			}
		}

		// プレイヤー位置から遠くまで飛んだら終了
		if (Length(fireballPos_ - playerPos) > 20.0f) {
			isFireballActive_ = false;
		}
	}
	// 敵が使った火球
	else {

		if (player && !player->IsDead()) {
			Vector3 diff = {
				playerPos.x - fireballPos_.x,
				playerPos.y - fireballPos_.y,
				playerPos.z - fireballPos_.z
			}; // プレイヤーとの距離

			if (Length(diff) < 1.5f) {
				player->TakeDamage(1, fireballPos_); // プレイヤーにダメージ
				isFireballActive_ = false;           // ヒットしたら終了
				return;
			}
		}

		// 敵位置から遠くまで飛んだら終了
		if (Length(fireballPos_ - enemyPos) > 20.0f) {
			isFireballActive_ = false;
		}
	}
}