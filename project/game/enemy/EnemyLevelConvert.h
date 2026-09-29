#pragma once
// =====================================================================
//  EnemyLevelConvert：敵の配置データの変換
//   （エディタ用 EnemySpawnData ⇔ マップ保存用 LevelEnemyData）。
//   engine 側は game の型を知らないので、同じ項目を持つ別の構造体へ写して渡す。
//   項目を増やした時はこのファイルの ToLevelEnemy / ToSpawnData の2つだけ直せばよい
// =====================================================================
#include "engine/utils/Level/LevelData.h"
#include "game/enemy/Enemy.h"

#include <vector>

namespace EnemyLevelConvert {
    LevelEnemyData ToLevelEnemy(const EnemySpawnData& spawn);
    EnemySpawnData ToSpawnData(const LevelEnemyData& data);

    std::vector<LevelEnemyData> ToLevelEnemies(const std::vector<EnemySpawnData>& spawnDatas);
    std::vector<EnemySpawnData> ToSpawnDatas(const std::vector<LevelEnemyData>& datas);
}
