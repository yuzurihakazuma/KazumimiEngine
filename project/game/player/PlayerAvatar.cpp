#include "PlayerAvatar.h"
#include "game/player/Player.h"
#include "game/player/SwallowAbility.h"
#include "game/player/AimThrowController.h"
#include "game/egg/EggSystem.h"
#include "game/rail/RailField.h"
#include "engine/3d/obj/Obj3d.h"
#include "engine/3d/obj/SkinnedObj3d.h"
#include "engine/base/TimeManager.h"

#include <algorithm>
#include <cmath>

PlayerAvatar::PlayerAvatar() = default;
PlayerAvatar::~PlayerAvatar() = default;

void PlayerAvatar::Initialize(uint32_t envMapSrv){
	// 7色はパレットPNGに焼き込み済みの1テクスチャ構成。クリップは Idle/TongueOut/Walk ほか
	model_ = SkinnedObj3d::Create("player", "resources/player", "player.gltf");
	if ( model_ ) { // モデル未登録だと nullptr が返る（Create は FindModel 前提）
		model_->SetEnvironmentMap(envMapSrv);
		model_->LoadClips("resources/player", "player.gltf");
		model_->SetClip("Idle", true);
	}
	// 構え中に手（Itemジョイント）へ持たせる卵の見た目
	heldEgg_ = Obj3d::Create("egg");
	prevPosValid_ = false;
}

void PlayerAvatar::Update(const Context& context){
	if ( !model_ ) return;

	// 立ち位置：Play中はプレイヤーに追従、Edit中はスタート地点のレール上に立たせて表示する
	//   （起動直後から「どこから始まるか」が一目で分かる。レール編集にも毎フレーム追従する）
	Vector3 pos {}, rot {};
	if ( context.playing && context.player ) {
		pos = context.player->GetPosition();
		rot = context.player->GetRotation();
	} else if ( context.railField ) {
		const auto& rails = context.railField->GetRails();
		int startRail = context.railField->GetStartRail();
		if ( startRail >= 0 && startRail < ( int ) rails.size() && rails[startRail].nodes.size() >= 2 ) {
			float startDistance = context.railField->GetStartDistance();
			pos = rails[startRail].GetPositionByDistance(startDistance);
			Vector3 tangent = rails[startRail].GetTangentByDistance(startDistance);
			if ( std::abs(tangent.x) > 1e-4f || std::abs(tangent.z) > 1e-4f ) {
				rot.y = std::atan2(tangent.x, tangent.z); // レールの進行方向を向かせる
			}
		}
	}

	// 実際に動いた速さ（見た目の位置の差分）→ クリップと再生速度を決める
	float horizontalSpeed = 0.0f;
	if ( prevPosValid_ ) {
		float dx = pos.x - prevPos_.x;
		float dz = pos.z - prevPos_.z;
		horizontalSpeed = std::sqrt(dx * dx + dz * dz) * 60.0f; // 固定60FPS想定で m/s へ
		// 向いている方へ動いたか（構えたまま後ろへ下がる時は、足を逆回しにする）
		if ( std::abs(dx) + std::abs(dz) > 1e-5f ) {
			moveForward_ = ( dx * std::sin(rot.y) + dz * std::cos(rot.y) ) >= 0.0f ? 1.0f : -1.0f;
		}
	}
	frameSeconds_ = Time::GetInstance()->GetDeltaTime();
	prevPos_ = pos;
	prevPosValid_ = true;
	const float kWalkBaseSpeed = 5.0f; // プレイヤーの通常移動速度(m/s)で Walk を等速再生
	float speedRatio = std::clamp(horizontalSpeed / kWalkBaseSpeed, 0.0f, 1.8f);

	// 構えをXでキャンセルした場合は投げモーションを出さない（卵は後ろへ戻るだけ）
	if ( context.aimThrow && context.aimThrow->ConsumeCanceled() ) { throwTimer_ = 0.0f; }

	SelectClip(context, speedRatio);

	// 跳ぶ・着く時の伸び縮み：跳んだ瞬間に縦に伸び、着いた瞬間にぽよんとつぶれる（どこで跳んで、どこへ着いたかが分かるように）
	if ( context.playing && context.player ) {
		const bool grounded = context.player->IsGrounded();
		if ( wasGrounded_ && !grounded ) { jumpStretch_ = 1.0f; }
		if ( !wasGrounded_ && grounded ) { landSquash_ = 1.0f; jumpStretch_ = 0.0f; }
		wasGrounded_ = grounded;
	} else {
		wasGrounded_ = true;
		jumpStretch_ = landSquash_ = 0.0f;
	}
	jumpStretch_ = ( std::max )( 0.0f, jumpStretch_ - frameSeconds_ / 0.22f );
	landSquash_ = ( std::max )( 0.0f, landSquash_ - frameSeconds_ / 0.18f );
	const float actionY = ( 1.0f + 0.16f * jumpStretch_ ) * ( 1.0f - 0.22f * std::sin(landSquash_ * 3.14159265f * 0.5f) );

	model_->SetTranslation({ pos.x, pos.y + modelYOffset_ + dropLift_, pos.z });
	const float scaleY = dropSquash_ * actionY;
	const float bulge = 1.0f + ( 1.0f - scaleY ) * 0.6f; // 縦につぶれた分だけ横へふくらむ（伸びた時は細く）
	model_->SetScale({ bulge, scaleY, bulge });
	model_->SetRotation({ defeatTilt_, defeatHasYaw_ ? defeatYaw_ : rot.y, defeatRoll_ });
	model_->Update();
}

void PlayerAvatar::PlayIntroClip(const std::string& clip){
	introClip_ = clip;
	introRestart_ = true;
}

void PlayerAvatar::ClearIntroClip(){
	introClip_.clear();
	introRestart_ = false;
}

bool PlayerAvatar::GetJointWorldPosition(const std::string& joint, Vector3& out) const{
	return model_ && model_->GetJointWorldPosition(joint, out);
}

void PlayerAvatar::ResetTongueAndHeldEgg(){
	model_->ClearBoneScaleOverride();
	model_->ClearLayer();
	tongueSeeked_ = false;
	heldEggVisible_ = false;
}

// クリップ選択：ベロ動作中 > 卵の構え > 投げた直後 > 産卵 > 空中 > 歩き > 待機
void PlayerAvatar::SelectClip(const Context& context, float speedRatio){
	const bool playing = context.playing;
	const bool aiming  = context.aimThrow && context.aimThrow->IsAiming();

	// ステージ開始演出のクリップが指定されていたら、いちばん先にそれを流す（状態から選び直すと Idle に戻されるため）
	if ( !introClip_.empty() ) {
		ResetTongueAndHeldEgg();
		model_->SetClip(introClip_, false);   // ループなし：最後の形で止まる
		model_->SetPlaybackSpeed(1.0f);
		if ( introRestart_ ) {                // SetClip は同じ名前だと時間を戻さないので、指定した瞬間に頭へ
			model_->SetAnimationTime(0.0f);
			introRestart_ = false;
		}
		return;
	}

	if ( playing && context.swallow && context.swallow->IsTongueActive() ) {
		bool wasRetracting = ( model_->GetCurrentClip() == "TongueOut" );
		model_->SetClip("TongueOut", false); // 予備動作→射出→収納（1回きり）
		model_->SetPlaybackSpeed(2.8f);      // 1.33秒のクリップを高速化したベロ動作(約0.5秒)に合わせる
		// ベロの長さを実距離に同期（Tongueボーンの伸びを上書き）＝目の前の敵でも貫通しない
		float stretch = std::clamp(context.swallow->GetTongueLength() / 1.2f, 0.10f, 1.0f);
		model_->SetBoneScaleOverride("Tongue", { 1.0f, stretch, 1.0f });
		model_->ClearLayer();
		// 捕獲成立で戻しに入った瞬間、クリップを収納パートへ飛ばす（伸ばす絵をスキップ）
		if ( wasRetracting && context.swallow->IsRetracting() && !tongueSeeked_ ) {
			model_->SetAnimationTime(0.85f);
			tongueSeeked_ = true;
		}
	} else if ( playing && aiming ) {
		// 卵投げの構え：Throw クリップの「溜め」ポーズで静止（両手で頭上に構える）
		model_->ClearBoneScaleOverride();
		tongueSeeked_ = false;
		model_->SetClip("Throw", false);
		model_->SetPlaybackSpeed(0.0f);
		model_->SetAnimationTime(0.67f); // 溜め(f16)のポーズ
		throwTimer_ = 0.65f;             // 離した後にリリース→復帰を再生する時間
		// 構えたまま歩く：体は構えのまま、足としっぽだけ歩く（後ろへ下がる時は逆回し）。止まれば構えの足に戻す
		//   （足が止まったまま滑っているように見えていた）
		if ( speedRatio > 0.05f ) {
			aimWalkTime_ += frameSeconds_ * ( std::max )( speedRatio, 0.5f ) * 1.3f * moveForward_;
			model_->SetLayer("Walk", aimWalkTime_, { "Foot_L", "Foot_R", "Tail" });
		} else {
			model_->ClearLayer();
		}
		// 構え中は卵を持ち物ソケット（Itemジョイント）に持たせる
		Vector3 itemPos;
		heldEggVisible_ = context.eggSystem && ( context.eggSystem->HeldCount() > 0 )
		               && model_->GetJointWorldPosition("Item", itemPos);
		if ( heldEggVisible_ && heldEgg_ ) {
			heldEgg_->SetTranslation(itemPos);
			heldEgg_->SetScale({ 0.29f, 0.29f, 0.29f }); // READMEの推奨（手のサイズに合う）
			heldEgg_->Update();
		}
	} else if ( playing && throwTimer_ > 0.0f ) {
		// 離した直後：リリース(f20)→復帰を最後まで再生
		heldEggVisible_ = false;
		model_->ClearLayer();
		if ( model_->GetCurrentClip() == "Throw" && throwTimer_ >= 0.649f ) {
			model_->SetAnimationTime(0.75f); // リリース直前(f18)から
		}
		model_->SetClip("Throw", false);
		model_->SetPlaybackSpeed(1.6f);
		throwTimer_ -= 1.0f / 60.0f;
	} else if ( playing && context.eggSystem && context.eggSystem->IsBirthActive() ) {
		// 産卵：しゃがみ踏ん張り→ポンッ（演出時間に合わせて早回し）
		heldEggVisible_ = false;
		model_->ClearLayer();
		model_->SetClip("EggLay", false);
		model_->SetPlaybackSpeed(2.5f);
	} else if ( playing && context.player && !context.player->IsGrounded() ) {
		ResetTongueAndHeldEgg();
		if ( context.player->IsFluttering() ) {
			model_->SetClip("Walk", true);   // ふんばり：足を高速バタバタ（ソニック風）
			model_->SetPlaybackSpeed(3.2f);
		} else {
			// ジャンプ中：Walkの足を伸ばした瞬間のポーズで静止＝つま先立ちで跳んでいる感じ
			model_->SetClip("Walk", true);
			model_->SetPlaybackSpeed(0.0f);
			model_->SetAnimationTime(0.25f); // クリップ(1秒)内のポーズ位置。好みで0〜1秒
		}
	} else if ( speedRatio > 0.05f ) {
		ResetTongueAndHeldEgg();
		model_->SetClip("Walk", true);       // 足パタパタ＋バウンド
		model_->SetPlaybackSpeed(( std::max )( speedRatio, 0.4f ));
	} else {
		ResetTongueAndHeldEgg();
		model_->SetClip("Idle", true);       // 呼吸＋頭の微揺れ
		model_->SetPlaybackSpeed(1.0f);
	}
}

void PlayerAvatar::Draw(bool playing, const Player* player){
	// 敵にぶつかった後の無敵中は点滅させる（約0.07秒ごとに消える）
	bool blinkHidden = false;
	if ( playing && player && player->IsInvincible() ) {
		blinkHidden = ( ( ++blinkFrame_ / 4 ) % 2 ) == 1;
	}
	if ( model_ && !blinkHidden ) { model_->Draw(); }
	if ( heldEggVisible_ && heldEgg_ ) { heldEgg_->Draw(); } // 構え中の手持ち卵
}
