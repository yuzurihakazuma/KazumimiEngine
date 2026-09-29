#include "EnemyLevelConvert.h"

#include <algorithm>

namespace EnemyLevelConvert {

LevelEnemyData ToLevelEnemy(const EnemySpawnData& spawn){
	LevelEnemyData data;
	data.type          = static_cast<int>( spawn.type );
	data.railIndex     = spawn.railIndex;
	data.distance      = spawn.distance;
	data.patrol        = spawn.patrol ? 1 : 0;
	data.patrolMin     = spawn.patrolMin;
	data.patrolMax     = spawn.patrolMax;
	data.name          = spawn.name;
	data.speed         = spawn.speed;
	data.startDir      = spawn.startDir;
	data.turnWait      = spawn.turnWait;
	data.chaseRange    = spawn.chaseRange;
	data.chaseSpeedMul = spawn.chaseSpeedMul;
	data.hoverHeight   = spawn.hoverHeight;
	data.bobAmp        = spawn.bobAmp;
	data.bobSpeed      = spawn.bobSpeed;
	data.biteRange     = spawn.biteRange;
	data.scale         = spawn.scale;
	return data;
}

EnemySpawnData ToSpawnData(const LevelEnemyData& data){
	EnemySpawnData spawn;
	spawn.type          = static_cast<EnemyType>( std::clamp(data.type, 0, 2) );
	spawn.railIndex     = data.railIndex;
	spawn.distance      = data.distance;
	spawn.patrol        = ( data.patrol != 0 );
	spawn.patrolMin     = data.patrolMin;
	spawn.patrolMax     = data.patrolMax;
	spawn.name          = data.name;
	spawn.speed         = data.speed;
	spawn.startDir      = data.startDir;
	spawn.turnWait      = data.turnWait;
	spawn.chaseRange    = data.chaseRange;
	spawn.chaseSpeedMul = data.chaseSpeedMul;
	spawn.hoverHeight   = data.hoverHeight;
	spawn.bobAmp        = data.bobAmp;
	spawn.bobSpeed      = data.bobSpeed;
	spawn.biteRange     = data.biteRange;
	spawn.scale         = data.scale;
	return spawn;
}

std::vector<LevelEnemyData> ToLevelEnemies(const std::vector<EnemySpawnData>& spawnDatas){
	std::vector<LevelEnemyData> datas;
	datas.reserve(spawnDatas.size());
	for ( const auto& spawnData : spawnDatas ) { datas.push_back(ToLevelEnemy(spawnData)); }
	return datas;
}

std::vector<EnemySpawnData> ToSpawnDatas(const std::vector<LevelEnemyData>& datas){
	std::vector<EnemySpawnData> spawnDatas;
	spawnDatas.reserve(datas.size());
	for ( const auto& data : datas ) { spawnDatas.push_back(ToSpawnData(data)); }
	return spawnDatas;
}

} // namespace EnemyLevelConvert
