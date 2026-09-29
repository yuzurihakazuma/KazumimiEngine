#include "EnemyRailPin.h"
#include "game/enemy/EnemyEditor.h"
#include "engine/rail/SplineRail.h"
#include "engine/math/VectorMath.h"

using namespace VectorMath;

void EnemyRailPin::Capture(const EnemyEditor& enemyEditor, const std::vector<SplineRail>& oldRails){
	positions_.clear();
	valid_.clear();
	for ( const auto& spawnData : enemyEditor.GetSpawnDatas() ) {
		bool railValid = ( spawnData.railIndex >= 0 && spawnData.railIndex < ( int ) oldRails.size()
		                && oldRails[spawnData.railIndex].nodes.size() >= 2 );
		valid_.push_back(railValid ? 1 : 0);
		positions_.push_back(railValid ? oldRails[spawnData.railIndex].GetPositionByDistance(spawnData.distance)
		                               : Vector3 { 0.0f, 0.0f, 0.0f });
	}
}

bool EnemyRailPin::Apply(EnemyEditor& enemyEditor, const std::vector<SplineRail>& newRails){
	// 張り直す前に、まだ敵の履歴に積んでいない利用者の変更があるかを見ておく
	const bool historyClean = enemyEditor.IsHistoryClean();
	bool repinned = false;
	auto& spawnDatas = enemyEditor.MutableSpawnDatas();
	for ( size_t i = 0; i < spawnDatas.size() && i < valid_.size(); ++i ) {
		if ( !valid_[i] ) continue;
		auto& spawnData = spawnDatas[i];
		if ( spawnData.railIndex < 0 || spawnData.railIndex >= ( int ) newRails.size() ) continue;
		const SplineRail& newRail = newRails[spawnData.railIndex];
		if ( newRail.nodes.size() < 2 ) continue;
		// レールがこの敵の下で動いていなければ、距離はそのまま（1cm 未満の違いは同じ場所とみなす）。
		//   張り直すと最寄り点探しの刻み（数cm〜数十cm）に丸められ、置いた 0.5m の目盛りから外れてしまう
		if ( spawnData.distance <= newRail.GetLength() ) {
			Vector3 samePlace = newRail.GetPositionByDistance(spawnData.distance);
			Vector3 gap = { samePlace.x - positions_[i].x, samePlace.y - positions_[i].y, samePlace.z - positions_[i].z };
			if ( Length(gap) < 0.01f ) continue;
		}
		float repinnedDist = newRail.GetClosestDistance(positions_[i]);
		if ( repinnedDist != spawnData.distance ) {
			spawnData.distance = repinnedDist;
			repinned = true;
		}
	}
	if ( repinned && historyClean ) { enemyEditor.RebaseHistory(); }
	positions_.clear();
	valid_.clear();
	return repinned;
}
