#pragma once
// =====================================================================
//  TitleTopple：タイトルの段ボールが「バタバタ倒れる」動き（紙芝居遷移の退場＝起き倒し）。
//   恐竜の足元から近い順に、奥へ倒れる（手前に倒すとカメラにめり込むので、向きは奥にそろえる）。
//   倒れる時は加速して、地面で小さく跳ね返ってから寝る。
//   背景（TitleAmbience）・ロゴの看板・メニューの的で同じ動きを使う
// =====================================================================
#include "engine/utils/Easing.h"

#include <cmath>

namespace TitleTopple {
	inline constexpr float kFallTime      = 0.6f;   // 倒れ始めてから寝るまで(秒)
	inline constexpr float kDelayPerMeter = 0.045f; // 恐竜から 1m 離れるごとの遅れ(秒)
	inline constexpr float kFlatAngle     = 1.55f;  // 寝た時の角度(rad)。真横(90°)より少し手前で止める（地面に埋まらない）

	// 倒れ始めてからの秒数 → 倒れた角度
	inline float Angle(float elapsed){
		if ( elapsed <= 0.0f ) { return 0.0f; }
		return kFlatAngle * Easing::EaseOutBounce(elapsed / kFallTime);
	}
	// 初めて地面に着いた瞬間か（EaseOutBounce は 1/2.75 で最初に 1 へ届く）。音を鳴らす合図
	inline bool JustLanded(float previousElapsed, float elapsed){
		const float landing = kFallTime / 2.75f;
		return previousElapsed < landing && elapsed >= landing;
	}
	// 世界の奥（+Z）へ倒すための X 回転。向き（yaw）が 180° の物は X 回転の向きが逆になる
	inline float RotationX(float angle, float yaw){ return angle * std::cos(yaw); }
}
