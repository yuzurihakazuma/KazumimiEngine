#include "PlayScreenEffects.h"
#include "engine/base/TimeManager.h"
#include "game/combat/CombatSystem.h"
#include "game/egg/EggSystem.h"
#include "engine/postEffect/PostEffect.h"
#include "engine/camera/Camera.h"
#include "engine/math/Matrix4x4.h"

#include <algorithm>

using namespace KazumimiEngine;

using namespace MatrixMath;

namespace {
	// 画面の上下のぼかし（ジオラマ風）：強さ(px)・ピントが合う帯の真ん中（画面の上 0〜下 1）・帯の半分の幅
	constexpr float kTiltStrength = 7.2f, kTiltCenterY = 0.55f, kTiltHalfWidth = 0.17f;
	constexpr float kTiltFadeTime = 0.6f; // ぼかしを消す・戻す時間(秒)
}

void PlayScreenEffects::UpdateTiltFocus(bool sharp, float deltaTime){
	const float target = sharp ? 0.0f : 1.0f;
	if ( tiltScale_ == target ) { return; }
	const float step = ( kTiltFadeTime > 0.0f ) ? deltaTime / kTiltFadeTime : 1.0f;
	tiltScale_ = ( tiltScale_ < target ) ? ( std::min )( tiltScale_ + step, target ) : ( std::max )( tiltScale_ - step, target );
	PostEffect::GetInstance()->SetTiltShiftParams(kTiltStrength * tiltScale_, kTiltCenterY, kTiltHalfWidth);
}

void PlayScreenEffects::EnablePlayLook(){
	PostEffect* postEffect = PostEffect::GetInstance();
	postEffect->SetEffectActive(PostEffectType::TiltShift, true);   // ジオラマ風（遠景ぼかし）
	postEffect->SetEffectActive(PostEffectType::PaperGrain, true);  // 紙の質感
	postEffect->SetEffectActive(PostEffectType::PictureBook, true); // 絵本風（ポスタライズ＋輪郭）
	postEffect->SetTiltShiftParams(kTiltStrength, kTiltCenterY, kTiltHalfWidth);
	tiltScale_ = 1.0f;
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

// =====================================================================
//  落下ミスのアイリスの、段階ごとの状態クラス（閉じる → 閉じたまま一呼吸 → 開く）。
//   円の半径と、次の段階へ進むかを、各状態が決める
// =====================================================================
class PlayScreenEffects::IrisState {
public:
	virtual ~IrisState() = default;
	virtual bool IsActive() const { return true; }
	// このフレームの円の半径を返す（1.4＝全開）。時間が来たら次の段階へ進める
	virtual float Step(PlayScreenEffects& /*effects*/, PostEffect& /*postEffect*/) const { return kOpenRadius; }

protected:
	static constexpr float kOpenRadius = 1.4f; // 全開
	static constexpr float kCloseTime = 0.18f, kHoldTime = 0.12f, kOpenTime = 0.5f;
};

// 状態クラスの入れ物（PlayScreenEffects の中の物に触れるよう、PlayScreenEffects の中に置く）
struct PlayScreenEffects::Irises {
	// IrisPhase の番号と同じ並びの表
	static const IrisState& Of(IrisPhase phase);

	// 出していない
	class None final : public IrisState {
	public:
		bool IsActive() const override { return false; }
	};

	// 閉じていく
	class Closing final : public IrisState {
	public:
		float Step(PlayScreenEffects& effects, PostEffect& /*postEffect*/) const override {
			if ( effects.irisTimer_ >= kCloseTime ) {
				effects.irisPhase_ = IrisPhase::Hold;
				effects.irisTimer_ = 0.0f;
				return 0.0f;
			}
			return kOpenRadius * ( 1.0f - effects.irisTimer_ / kCloseTime );
		}
	};

	// 閉じたまま一呼吸。開き始める瞬間に色を戻す（円の外から色付きの世界が現れる）
	class Hold final : public IrisState {
	public:
		float Step(PlayScreenEffects& effects, PostEffect& postEffect) const override {
			if ( effects.irisTimer_ >= kHoldTime ) {
				effects.irisPhase_ = IrisPhase::Opening;
				effects.irisTimer_ = 0.0f;
				postEffect.SetEffectActive(PostEffectType::Grayscale, false);
			}
			return 0.0f;
		}
	};

	// 開いていく（ゆっくり始まってすっと開く）。開き切ったら終わり
	class Opening final : public IrisState {
	public:
		float Step(PlayScreenEffects& effects, PostEffect& postEffect) const override {
			const float t = effects.irisTimer_ / kOpenTime;
			if ( effects.irisTimer_ >= kOpenTime ) {
				effects.irisPhase_ = IrisPhase::None;
				postEffect.SetEffectActive(PostEffectType::IrisWipe, false);
			}
			return kOpenRadius * t * t;
		}
	};
};

const PlayScreenEffects::IrisState& PlayScreenEffects::Irises::Of(IrisPhase phase){
	static const None none {};
	static const Closing closing {};
	static const Hold hold {};
	static const Opening opening {};
	static const IrisState* const kTable[] = { &none, &closing, &hold, &opening };
	return *kTable[static_cast<size_t>( phase )];
}

void PlayScreenEffects::UpdateIris(const Vector3& focus, const Camera& camera){
	const IrisState& state = Irises::Of(irisPhase_);
	if ( !state.IsActive() ) return;
	PostEffect* postEffect = PostEffect::GetInstance();
	irisTimer_ += Time::kFixedDeltaTime;
	const float radius = state.Step(*this, *postEffect); // 半径と、次の段階へ進むかは、状態が決める
	// 円の中心をスクリーンUVへ投影
	float centerU = 0.5f, centerV = 0.5f;
	Vector2 ndc;
	if ( WorldToNdc(focus, camera.GetViewProjectionMatrix(), ndc) ) {
		centerU = ndc.x * 0.5f + 0.5f;
		centerV = 1.0f - ( ndc.y * 0.5f + 0.5f );
	}
	postEffect->SetIrisParams(( std::max )( radius, 0.0f ), centerU, centerV);
}

void PlayScreenEffects::UpdateWarmup(CombatSystem& combat, EggSystem& eggs, bool playing){
	if ( warmupFrame_ > 4 ) return;
	++warmupFrame_;
	PostEffect* postEffect = PostEffect::GetInstance();
	if ( warmupFrame_ == 1 ) {
		eggs.WarmupBirthFx(); // 画面外で産卵の SDF の卵を描かせる（SDF の初回描画の固まりを前倒し）
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
