#pragma once

#include "engine/utils/Level/LevelData.h"
#include "engine/math/VectorMath.h"

#include <vector>
#include <utility>
#include <random>

class SpawnManager {
public:
	SpawnManager();
	~SpawnManager() = default;

	// 参照するマップデータをセット
	void SetLevelData(const LevelData* levelData);

	// 基本判定
	bool HasLevelData() const;
	bool IsInside(int x, int z) const;
	bool IsFloorTile(int x, int z) const;
	bool IsWallTile(int x, int z) const;

	// 周囲marginマス以内に壁がないかを判定
	bool HasEnoughSpaceFromWalls(int x, int z, int margin) const;

	// 敵配置可能か
	bool CanSpawnEnemy(int x, int z, int margin) const;

	// カード配置可能か
	bool CanSpawnCard(int x, int z, int margin) const;

	// 候補一覧取得
	std::vector<std::pair<int, int>> FindEnemySpawnCandidates(int margin) const;
	std::vector<std::pair<int, int>> FindCardSpawnCandidates(int margin) const;

	// ランダムで1つ取得
	bool GetRandomEnemyTile(int margin, int& outX, int& outZ) const;
	bool GetRandomCardTile(int margin, int& outX, int& outZ) const;

	// タイル座標 → ワールド座標
	Vector3 TileToWorldPosition(int x, int z, float y = 0.0f) const;

private:
	const LevelData* levelData_ = nullptr;

	// mutable にして const関数内でも乱数を使えるようにする
	mutable std::mt19937 randomEngine_;
};