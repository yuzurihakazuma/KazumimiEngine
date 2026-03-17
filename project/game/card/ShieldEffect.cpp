#include "ShieldEffect.h"
#include "game/player/Player.h"

void ShieldEffect::Start(const Vector3 &casterPos, float casterYaw, bool isPlayerCaster, Camera *camera) {
	isPlayerCaster_ = isPlayerCaster;
	isFinished_ = false;
	timer_ = duration_;

	// シールド用オブジェクト生成
	obj_ = Obj3d::Create("sphere");
	if (obj_) {
		obj_->SetCamera(camera);
		obj_->SetScale(scale_);
		obj_->SetTranslation(casterPos);
		obj_->Update();
	}
}

void ShieldEffect::Update(Player *player, Enemy *enemy, Boss *boss, const Vector3 &enmeyPos, const Vector3 &bossPos, const LevelData &level) {

	if (isFinished_) {
		return;
	}

	timer_--;
	if (timer_ <= 0) {
		isFinished_ = true;
		return;
	}

	// プレイヤーが使った場合はプレイヤーにシールド効果を付与し、モデルを追従させる
	if (isPlayerCaster_ && player != nullptr && !player->IsDead()) {
		// 最初の１フレーム目だけプレイヤーのシールドフラグをオンにする
		if (timer_ == duration_ - 1) {
			player->ActivateShield(duration_);
		}

		// シールドのモデルをプレイヤーの位置にピッタリ合わせる
		if (obj_) {
			obj_->SetTranslation(player->GetPosition());
			obj_->Update();
		}

	}


}

void ShieldEffect::Draw() {
	if (!isFinished_ && obj_) {
		obj_->Draw();
	}
}
