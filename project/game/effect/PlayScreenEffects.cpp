#include "PlayScreenEffects.h"
#include "game/combat/CombatSystem.h"
#include "engine/postEffect/PostEffect.h"
#include "engine/camera/Camera.h"
#include "engine/math/Matrix4x4.h"

#include <algorithm>

using namespace MatrixMath;

void PlayScreenEffects::EnablePlayLook(){
	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->SetEffectActive(PostEffectType::TiltShift, true);   // ジオラマ風（遠景ぼかし）
	postEffect->SetEffectActive(PostEffectType::PaperGrain, true);  // 紙の質感
	postEffect->SetEffectActive(PostEffectType::PictureBook, true); // 絵本風（ポスタライズ＋輪郭）
	postEffect->SetTiltShiftParams(7.2f, 0.55f, 0.17f);
	postEffect->SetPaperGrainStrength(0.17f);
	postEffect->SetPictureBookParams(14.0f, 0.8f);
}

void PlayScreenEffects::DisablePlayLook(){
	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->SetEffectActive(PostEffectType::TiltShift, false);
	postEffect->SetEffectActive(PostEffectType::PaperGrain, false);
	postEffect->SetEffectActive(PostEffectType::PictureBook, false);
	postEffect->SetEffectActive(PostEffectType::IrisWipe, false);
	postEffect->SetEffectActive(PostEffectType::Grayscale, false); // アイリス途中でEditへ戻った場合の消し忘れ防止
	irisPhase_ = IrisPhase::None;
}

void PlayScreenEffects::StartMissIris(){
	irisPhase_ = IrisPhase::Closing;
	irisTimer_ = 0.0f;
	PostEffect::GetInstance()->SetEffectActive(PostEffectType::IrisWipe, true);
	// ミスの瞬間は画面の色も抜く（グレースケール）。開く時に色が戻って「復活」感を出す
	PostEffect::GetInstance()->SetEffectActive(PostEffectType::Grayscale, true);
}

void PlayScreenEffects::UpdateIris(const Vector3& focus, const Camera& camera){
	if ( irisPhase_ == IrisPhase::None ) return;
	PostEffect* postEffect = PostEffect::GetInstance();
	irisTimer_ += 1.0f / 60.0f;
	const float kCloseTime = 0.18f, kHoldTime = 0.12f, kOpenTime = 0.5f;
	float radius = 1.4f; // 全開
	if ( irisPhase_ == IrisPhase::Closing ) {
		radius = 1.4f * ( 1.0f - irisTimer_ / kCloseTime );
		if ( irisTimer_ >= kCloseTime ) { irisPhase_ = IrisPhase::Hold; irisTimer_ = 0.0f; radius = 0.0f; }
	} else if ( irisPhase_ == IrisPhase::Hold ) { // 閉じたまま一呼吸
		radius = 0.0f;
		if ( irisTimer_ >= kHoldTime ) {
			irisPhase_ = IrisPhase::Opening; irisTimer_ = 0.0f;
			// 開き始めた瞬間に色を戻す（円の外から色付きの世界が現れる）
			postEffect->SetEffectActive(PostEffectType::Grayscale, false);
		}
	} else {
		float t = irisTimer_ / kOpenTime;
		radius = 1.4f * t * t; // ゆっくり始まってすっと開く
		if ( irisTimer_ >= kOpenTime ) {
			irisPhase_ = IrisPhase::None;
			postEffect->SetEffectActive(PostEffectType::IrisWipe, false);
		}
	}
	// 円の中心をスクリーンUVへ投影
	float centerU = 0.5f, centerV = 0.5f;
	Vector2 ndc;
	if ( WorldToNdc(focus, camera.GetViewProjectionMatrix(), ndc) ) {
		centerU = ndc.x * 0.5f + 0.5f;
		centerV = 1.0f - ( ndc.y * 0.5f + 0.5f );
	}
	postEffect->SetIrisParams(( std::max )( radius, 0.0f ), centerU, centerV);
}

void PlayScreenEffects::UpdateWarmup(CombatSystem& combat, bool playing){
	if ( warmupFrame_ > 4 ) return;
	++warmupFrame_;
	PostEffect* postEffect = PostEffect::GetInstance();
	if ( warmupFrame_ == 1 ) {
		combat.WarmupDissolveFx(); // 画面外でSDF溶けを1回描かせる
		postEffect->SetMaskParams(PostEffectMaskParams{});   // マスク半径0＝画面に影響なし
		postEffect->SetEffectActive(PostEffectType::MaskedDistortion, true);
		postEffect->SetEffectActive(PostEffectType::MaskedGlow, true);
		postEffect->SetIrisParams(2.0f, 0.5f, 0.5f);         // 全開＝見えない
		postEffect->SetEffectActive(PostEffectType::IrisWipe, true);
		postEffect->SetEffectActive(PostEffectType::TiltShift, true);
		postEffect->SetEffectActive(PostEffectType::PaperGrain, true);
		postEffect->SetEffectActive(PostEffectType::PictureBook, true);
		postEffect->SetEffectActive(PostEffectType::Grayscale, true); // 落下ミス演出で使うので初回パスも前倒し
	} else if ( warmupFrame_ == 4 ) {
		// 3フレーム通したら全て元に戻す（Edit中は全OFFが正規状態。Play遷移時は改めてONになる）
		postEffect->SetEffectActive(PostEffectType::MaskedDistortion, false);
		postEffect->SetEffectActive(PostEffectType::MaskedGlow, false);
		postEffect->SetEffectActive(PostEffectType::IrisWipe, false);
		postEffect->SetEffectActive(PostEffectType::TiltShift, false);
		postEffect->SetEffectActive(PostEffectType::PaperGrain, false);
		postEffect->SetEffectActive(PostEffectType::PictureBook, false);
		postEffect->SetEffectActive(PostEffectType::Grayscale, false);
		combat.ClearEffects(); // 画面外の溶け演出を止めてスロットを返す
		// 最初から Play で始まった時（リリース版）は、ここで消したプレイ用の見た目を戻す
		if ( playing ) { EnablePlayLook(); }
	}
}
