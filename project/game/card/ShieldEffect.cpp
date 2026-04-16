#include "ShieldEffect.h"
#include "game/player/Player.h"
#include "game/enemy/EnemyManager.h"
#include "engine/particle/GPUParticleManager.h"

#include <cmath>


void ShieldEffect::Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera) {
	// 誰が使ったか（プレイヤーか敵か）を保存
	isPlayerCaster_ = isPlayerCaster;

	// 演出終了フラグをリセットし、タイマーを初期化
	isFinished_ = false;
	isFirstFrame_ = true;

	// シールド用オブジェクト生成
	obj_ = Obj3d::Create("sphere");
	if (obj_) {
		obj_->SetCamera(camera);
		obj_->SetScale(scale_);
		obj_->SetTranslation(casterPos);
		obj_->Update();
	}
}

void ShieldEffect::Update(Player *player, EnemyManager *enemyManager, Boss *boss,  const Vector3 &bossPos, const LevelData &level) {

	// すでに演出が終わっている場合は何もせずに返す
	if (isFinished_) {
		return;
	}
	if (isPlayerCaster_ && player != nullptr && !player->IsDead()) {

		// 最初の１フレーム目だけ、プレイヤー本体に「3回防ぐシールド」を付与
		if (isFirstFrame_) {
			player->AddShieldHits(3); // ここで3回分をセット！
			isFirstFrame_ = false;    // 2回目以降は呼ばないようにする
		}

		if ( player->GetShieldHits() <= 0 ) {
			// 四方八方にパーティクルを散らして「パキィッ！」という破砕感を出す
			for ( int i = 0; i < 30; i++ ) {
				float angleX = static_cast< float >(rand() % 628) * 0.01f;
				float angleY = static_cast< float >(rand() % 628) * 0.01f;

				Vector3 burstVel = {
					std::cosf(angleX) * std::cosf(angleY) * 0.5f,
					std::sinf(angleY) * 0.5f,
					std::sinf(angleX) * std::cosf(angleY) * 0.5f
				};

				Vector4 color = { 1.0f, 0.8f, 0.2f, 1.0f }; // 黄色・オレンジ系
				float lifeTime = 0.3f + static_cast< float >( rand() % 3 ) * 0.1f;
				float scale = 0.2f + static_cast< float >( rand() % 3 ) * 0.1f;

				GPUParticleManager::GetInstance()->Emit(player->GetPosition(), burstVel, lifeTime, scale, color);
			}

			isFinished_ = true;
			return;
		}

		rotAngle_ += 0.1f; // 回転速度（値を大きくすると速く回る）
		const int kOrbCount = 3; // 回る光の玉の数
		float radius = 1.2f;     // 回る軌道の半径（シールドの大きさscale_に合わせて調整）

		for ( int i = 0; i < kOrbCount; i++ ) {
			// 3つの玉を均等な間隔（120度ずつ）で配置
			float angle = rotAngle_ + ( 3.141592f * 2.0f / kOrbCount ) * i;

			Vector3 emitPos = {
				player->GetPosition().x + std::cosf(angle) * radius,
				player->GetPosition().y + 1.0f, // プレイヤーの腰〜胸くらいの高さ
				player->GetPosition().z + std::sinf(angle) * radius
			};

			// その場に少し留まりつつ、わずかにフワッと上がる速度
			Vector3 vel = { 0.0f, 0.05f, 0.0f };
			Vector4 color = { 1.0f, 0.8f, 0.2f, 1.0f }; // シールドっぽい黄色

			// 軌跡を描くように少しだけ寿命を持たせる
			float lifeTime = 0.2f;
			float scale = 0.3f;

			GPUParticleManager::GetInstance()->Emit(emitPos, vel, lifeTime, scale, color);
		}


		// プレイヤーのシールド残り回数が 0 になったら演出終了（シールド破壊）
		if (player->GetShieldHits() <= 0) {
			isFinished_ = true;
			return;
		}

		// シールドのモデルを、常にプレイヤーの現在位置にピッタリ追従させる
		if (obj_) {
			obj_->SetTranslation(player->GetPosition());
			obj_->Update();
		}

	}


}

void ShieldEffect::Draw() {
	// まだ演出が終わっておらず、モデルがちゃんと作られていれば描画する
	if (!isFinished_ && obj_) {
		obj_->Draw();
	}
}
