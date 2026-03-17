#include "ShieldEffect.h"
#include "game/player/Player.h"

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

void ShieldEffect::Update(Player *player, Enemy *enemy, Boss *boss, const Vector3 &enmeyPos, const Vector3 &bossPos, const LevelData &level) {

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
