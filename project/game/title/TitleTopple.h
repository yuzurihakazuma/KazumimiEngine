#pragma once
// =====================================================================
//  TitleTopple：タイトルの段ボールが「バタバタ倒れる」動き（紙芝居遷移の退場＝起き倒し）。
//   ・卵の衝撃の中心（恐竜の足元）から近い順に倒れる
//   ・中心から外へ吹き飛ばされるように倒れる：左にある物は左奥へ、右にある物は右奥へ
//     （手前に倒すとカメラにめり込むので、どれも奥向きの範囲にとどめる）
//   ・重さで倒れ方を変える：軽い草花はパタッと素早く、重い木や山はゆっくり傾いてから一気に叩きつける。
//     地面に着いたら、軽い物ほど大きく跳ね返る
//   背景（TitleAmbience）・ロゴの看板・メニューの的で同じ動きを使う
// =====================================================================
#include "engine/math/Matrix4x4.h"
#include "engine/math/struct.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace TitleTopple {
	inline constexpr float kDelayPerMeter = 0.045f; // 衝撃の中心から 1m 離れるごとの遅れ(秒)
	inline constexpr float kFlatAngle     = 1.55f;  // 寝た時の角度(rad)。真横(90°)より少し手前で止める（地面に埋まらない）
	inline constexpr float kSpread        = 6.0f;   // 中心から横にこれだけ離れた物は、一番外向きに倒れる(m)
	inline constexpr float kMaxSide       = 0.9f;   // 一番外向きに倒れる時の向き（真後ろから何 rad 横へ）
	inline constexpr float kSettleTime    = 0.45f;  // 地面に着いてから、跳ね返りが収まるまで(秒)
	inline constexpr float kPi            = 3.14159265358979323846f;

	// 倒れ方（重さ）
	struct Weight {
		float fallTime; // 倒れ始めてから地面に着くまで(秒)
		float power;    // 倒れ始めのゆっくりさ（大きいほど、ゆっくり傾いてから一気に倒れる＝重く見える）
		float bounce;   // 地面で跳ね返る大きさ（寝た角度に対する割合。軽い物ほど大きい）
	};
	inline constexpr Weight kLight  { 0.32f, 2.0f, 0.14f }; // 草・花
	inline constexpr Weight kMedium { 0.50f, 2.4f, 0.09f }; // 柵・紙コップ・石・メニューの的・紙の波
	inline constexpr Weight kHeavy  { 0.72f, 2.8f, 0.05f }; // 木・ロゴの看板
	inline constexpr Weight kHuge   { 0.95f, 3.2f, 0.03f }; // 山

	// マップの物の種類（モデル名）から重さを決める
	inline Weight WeightFor(const std::string& type){
		auto startsWith = [&](const char* prefix){ return type.rfind(prefix, 0) == 0; };
		if ( startsWith("grass_") || startsWith("flower_") ) { return kLight; }
		if ( startsWith("tree_") ) { return kHeavy; }
		if ( startsWith("hill_") ) { return kHuge; }
		return kMedium;
	}

	// 倒れ始めてからの秒数 → 倒れた角度
	inline float Angle(float elapsed, const Weight& weight){
		if ( elapsed <= 0.0f ) { return 0.0f; }
		if ( elapsed < weight.fallTime ) { return kFlatAngle * std::pow(elapsed / weight.fallTime, weight.power); }
		// 地面に着いた：小さく跳ね返って、2回ほどで収まる
		const float t = elapsed - weight.fallTime;
		const float rebound = weight.bounce * std::exp(-t * 7.0f) * std::abs(std::sin(t * kPi / 0.16f));
		return kFlatAngle * ( 1.0f - rebound );
	}
	// 地面に着いた瞬間か（パタンの音の合図）
	inline bool JustLanded(float previousElapsed, float elapsed, const Weight& weight){
		return previousElapsed < weight.fallTime && elapsed >= weight.fallTime;
	}
	// 倒れ始めてから、動きが収まるまでの秒数
	inline float EndTime(const Weight& weight){ return weight.fallTime + kSettleTime; }

	// 倒れる向き（真後ろ=0、右奥が正）。中心から横にどれだけ離れているか（dx）で決める
	inline float Direction(float dx){ return std::clamp(dx / kSpread, -1.0f, 1.0f) * kMaxSide; }

	// 置いた向き（rotation）のまま、世界の direction の向きへ angle だけ根元を軸に倒した回転を返す。
	//   物の向き（Y 回転）に関係なく、決めた向きへ倒れる
	inline Vector3 Tilt(const Vector3& rotation, float direction, float angle){
		if ( angle == 0.0f ) { return rotation; }
		using namespace MatrixMath;
		// 置いた向き（エンジンの回転の順番：X → Y → Z）
		const Matrix4x4 placed = Multiply(Multiply(MakeRotateX(rotation.x), MakeRotateY(rotation.y)), MakeRotateZ(rotation.z));
		// 世界で倒す：倒す向きを奥(+Z)へ回してから X で倒し、元の向きへ戻す
		const Matrix4x4 tilt = Multiply(Multiply(MakeRotateY(-direction), MakeRotateX(angle)), MakeRotateY(direction));
		const Matrix4x4 m = Multiply(placed, tilt);
		// 回転行列 → X・Y・Z の角度（MakeAffine と同じ順番）
		Vector3 result;
		result.y = std::asin(std::clamp(-m.m[0][2], -1.0f, 1.0f));
		result.x = std::atan2(m.m[1][2], m.m[2][2]);
		result.z = std::atan2(m.m[0][1], m.m[0][0]);
		return result;
	}
}
