#include "SceneEditContext.h"
#include "game/enemy/Enemy.h"
#include "game/enemy/EnemyEditor.h"
#include "game/stage/CoinSystem.h"
#include "engine/rail/SplineRail.h"
#include "engine/math/Matrix4x4.h"

#include <algorithm>
#include <cmath>

using namespace MatrixMath;

bool SceneEditContext::ProjectToScreen(const Vector3& world, Vector2& out) const{
	const auto& view = View();
	Vector2 ndc;
	if ( !WorldToNdc(world, view.viewProj, ndc) ) return false;
	out.x = view.imgMin.x + ( ndc.x * 0.5f + 0.5f ) * view.imgSize.x;
	out.y = view.imgMin.y + ( 1.0f - ( ndc.y * 0.5f + 0.5f ) ) * view.imgSize.y;
	return true;
}

float SceneEditContext::ScreenDistanceToMouse(const Vector3& world) const{
	Vector2 screen;
	if ( !ProjectToScreen(world, screen) ) return 1e9f;
	float dx = screen.x - View().mousePos.x, dy = screen.y - View().mousePos.y;
	return std::sqrt(dx * dx + dy * dy);
}

bool SceneEditContext::RailPoint(int railIndex, float distance, float height, Vector3& out) const{
	const auto& rails = Rails();
	if ( railIndex < 0 || railIndex >= ( int ) rails.size() ) return false;
	if ( rails[railIndex].nodes.size() < 2 ) return false;
	Vector3 p = rails[railIndex].GetPositionByDistance(distance);
	out = { p.x, p.y + height, p.z };
	return true;
}

bool SceneEditContext::PickRailPoint(float height, float maxPx, int stickRail, float stickBonus,
                                     RailPick& out, int allowHidden) const{
	const auto& rails = Rails();
	int bestRail = -1; float bestDist = 0.0f; float bestPx = maxPx;
	for ( int railIndex = 0; railIndex < ( int ) rails.size(); ++railIndex ) {
		const SplineRail& rail = rails[railIndex];
		if ( rail.nodes.size() < 2 ) continue;
		// 見えない骨組み（リフトのガイド等）へは乗せない
		if ( !rail.visible && railIndex != allowHidden ) continue;
		const float length = rail.GetLength();
		const int steps = std::clamp(( int ) ( length / 0.5f ), 2, 400);
		for ( int s = 0; s <= steps; ++s ) {
			const float dist = length * ( float ) s / ( float ) steps;
			Vector3 p = rail.GetPositionByDistance(dist);
			float d = ScreenDistanceToMouse({ p.x, p.y + height, p.z });
			if ( railIndex == stickRail ) { d -= stickBonus; }
			if ( d < bestPx ) { bestPx = d; bestRail = railIndex; bestDist = dist; }
		}
	}
	if ( bestRail < 0 ) return false;
	out.rail = bestRail;
	out.dist = bestDist;
	return true;
}

bool SceneEditContext::EnemyWorldPos(const EnemySpawnData& spawnData, Vector3& out) const{
	return RailPoint(spawnData.railIndex, spawnData.distance, Enemy::PickHeightOf(spawnData), out);
}

int SceneEditContext::FindEnemyNearMouse(float maxPx) const{
	if ( !enemyEditor ) return -1;
	const auto& spawnDatas = enemyEditor->GetSpawnDatas();
	int found = -1;
	float best = maxPx;
	for ( int i = 0; i < ( int ) spawnDatas.size(); ++i ) {
		Vector3 wp;
		if ( !EnemyWorldPos(spawnDatas[i], wp) ) continue;
		float d = ScreenDistanceToMouse(wp);
		if ( d < best ) { best = d; found = i; }
	}
	return found;
}

void SceneEditContext::ResyncCoins() const{
	if ( coinSystem && editor ) { coinSystem->Sync(editor->GetEditorCoins(), Rails()); }
}
