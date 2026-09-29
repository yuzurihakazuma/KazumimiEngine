#include "RailEditOverlay.h"
#include "engine/rail/SplineRail.h"
#include "engine/graphics/DebugDraw.h"

#include <algorithm>
#include <cmath>

namespace {
	// レールに沿った折れ線（lift だけ持ち上げる）
	void DrawRailPolyline(const SplineRail& rail, int steps, float lift, const Vector4& color){
		float length = rail.GetLength();
		Vector3 prev = rail.GetPositionByDistance(0.0f);
		for ( int s = 1; s <= steps; ++s ) {
			Vector3 cur = rail.GetPositionByDistance(length * ( float ) s / ( float ) steps);
			DebugDraw::GetInstance()->Line({ prev.x, prev.y + lift, prev.z }, { cur.x, cur.y + lift, cur.z }, color);
			prev = cur;
		}
	}

	const Vector4 kGhostColor { 0.55f, 0.75f, 1.0f, 0.60f }; // 薄い水色＝動きの端
	const Vector4 kSweepColor { 0.55f, 0.75f, 1.0f, 0.28f }; // さらに薄い＝掃引の対応線

	// 円運動：始点・中間・終点の3箇所に軌道リング（実際に通る円/楕円）
	void DrawCircleMotion(const SplineRail& rail){
		const float kTwoPi = 2.0f * 3.14159265f;
		const float railLen = rail.GetLength();
		const Vector3& amp = rail.motionAmp;
		const float ringAnchors[3] = { 0.0f, 0.5f, 1.0f };
		for ( float anchor : ringAnchors ) {
			Vector3 center = rail.GetPositionByDistance(railLen * anchor);
			Vector3 prev {};
			for ( int k = 0; k <= 32; ++k ) {
				float th = ( float ) k / 32.0f * kTwoPi;
				Vector3 p = { center.x + amp.x * std::cos(th),
				              center.y + amp.y * std::sin(th),
				              center.z + amp.z * std::sin(th) };
				if ( k > 0 ) { DebugDraw::GetInstance()->Line(prev, p, kGhostColor); }
				prev = p;
			}
		}
	}

	// ガイドレール追従：レール始点が実際に通る経路（区間指定を反映して描く）
	void DrawGuideMotion(const std::vector<SplineRail>& rails, int railIndex){
		const SplineRail& rail = rails[railIndex];
		int g = rail.guideRail;
		if ( g < 0 || g >= ( int ) rails.size() || g == railIndex || rails[g].GetLength() <= 0.0f ) return;
		const SplineRail& guide = rails[g];
		const float guideLen = guide.GetLength();
		float s0 = std::clamp(rail.guideStart, 0.0f, guideLen);
		float s1 = ( rail.guideEnd < 0.0f ) ? guideLen : std::clamp(rail.guideEnd, 0.0f, guideLen);
		if ( s1 < s0 ) { std::swap(s0, s1); }
		if ( s1 - s0 < 0.01f ) { s0 = 0.0f; s1 = guideLen; }
		const int guideSteps = std::clamp(( int ) ( s1 - s0 ), 2, 150);
		Vector3 rangeStart = guide.GetPositionByDistance(s0);
		Vector3 railStart  = rail.GetPositionByDistance(0.0f);
		Vector3 prev {};
		for ( int k = 0; k <= guideSteps; ++k ) {
			Vector3 gp = guide.GetPositionByDistance(s0 + ( s1 - s0 ) * ( float ) k / ( float ) guideSteps);
			Vector3 p = { railStart.x + gp.x - rangeStart.x,
			              railStart.y + gp.y - rangeStart.y,
			              railStart.z + gp.z - rangeStart.z };
			if ( k > 0 ) { DebugDraw::GetInstance()->Line(prev, p, kGhostColor); }
			// 端の目印（ここで折り返す/戻る）：始端は緑・終端は橙の小さな十字
			if ( k == 0 || k == guideSteps ) {
				Vector4 endColor = ( k == 0 ) ? Vector4 { 0.4f, 1.0f, 0.5f, 0.9f }
				                              : Vector4 { 1.0f, 0.7f, 0.3f, 0.9f };
				DebugDraw::GetInstance()->Line({ p.x - 0.4f, p.y, p.z }, { p.x + 0.4f, p.y, p.z }, endColor);
				DebugDraw::GetInstance()->Line({ p.x, p.y - 0.4f, p.z }, { p.x, p.y + 0.4f, p.z }, endColor);
			}
			prev = p;
		}
	}

	// 0=サイン往復 / 1=停止つき往復：両端(+振幅/-振幅)のゴースト線＋数カ所のスイープ線
	void DrawSwingMotion(const SplineRail& rail){
		const float railLen = rail.GetLength();
		const int steps = std::clamp(( int ) railLen, 2, 100);
		const Vector3& amp = rail.motionAmp;
		Vector3 prevPlus {}, prevMinus {};
		const int sweepEvery = ( std::max )( steps / 4, 1 );
		for ( int s = 0; s <= steps; ++s ) {
			Vector3 p = rail.GetPositionByDistance(railLen * ( float ) s / ( float ) steps);
			Vector3 plus  = { p.x + amp.x, p.y + amp.y, p.z + amp.z };
			Vector3 minus = { p.x - amp.x, p.y - amp.y, p.z - amp.z };
			if ( s > 0 ) {
				DebugDraw::GetInstance()->Line(prevPlus,  plus,  kGhostColor);
				DebugDraw::GetInstance()->Line(prevMinus, minus, kGhostColor);
			}
			prevPlus = plus; prevMinus = minus;
			if ( s % sweepEvery == 0 ) { DebugDraw::GetInstance()->Line(minus, plus, kSweepColor); }
		}
	}
}

namespace RailEditOverlay {

void DrawMotionRanges(const std::vector<SplineRail>& rails){
	for ( int railIndex = 0; railIndex < ( int ) rails.size(); ++railIndex ) {
		const SplineRail& rail = rails[railIndex];
		if ( !rail.HasMotion() || rail.nodes.size() < 2 || !rail.visible ) continue;
		switch ( rail.motionType ) {
		case 2:  DrawCircleMotion(rail); break;
		case 3:  DrawGuideMotion(rails, railIndex); break;
		default: DrawSwingMotion(rail); break;
		}
	}
}

void DrawHiddenGuides(const std::vector<SplineRail>& rails, int skipGuide){
	std::vector<bool> guideDrawn(rails.size(), false);
	if ( skipGuide >= 0 && skipGuide < ( int ) rails.size() ) { guideDrawn[skipGuide] = true; }
	for ( const SplineRail& rail : rails ) {
		if ( rail.motionType != 3 ) continue;
		int guideIndex = rail.guideRail;
		if ( guideIndex < 0 || guideIndex >= ( int ) rails.size() || guideDrawn[guideIndex] ) continue;
		const SplineRail& guide = rails[guideIndex];
		if ( guide.visible || guide.nodes.size() < 2 || guide.GetLength() <= 0.0f ) continue;
		guideDrawn[guideIndex] = true;
		DrawRailPolyline(guide, std::clamp(( int ) guide.GetLength(), 2, 100), 0.0f, { 1.0f, 0.65f, 0.25f, 0.45f });
	}
}

void DrawAppearRoads(const std::vector<SplineRail>& rails){
	const Vector4 appearColor { 0.8f, 0.5f, 1.0f, 0.8f };
	for ( const SplineRail& rail : rails ) {
		if ( rail.appearTrigger < 0 || rail.nodes.size() < 2 ) continue;
		float appearLen = rail.GetLength();
		DrawRailPolyline(rail, std::clamp(( int ) appearLen, 2, 100), 0.12f, appearColor);
		// 発動レール → この道 へのリンク線（どこに乗れば現れるかが分かる）
		int trigger = rail.appearTrigger;
		if ( trigger < ( int ) rails.size() && rails[trigger].nodes.size() >= 2 ) {
			Vector3 from = rails[trigger].GetPositionByDistance(rails[trigger].GetLength() * 0.5f);
			Vector3 to   = rail.GetPositionByDistance(appearLen * 0.5f);
			DebugDraw::GetInstance()->Line({ from.x, from.y + 0.6f, from.z },
			                               { to.x, to.y + 0.6f, to.z }, { 0.8f, 0.5f, 1.0f, 0.35f });
		}
	}
}

} // namespace RailEditOverlay
