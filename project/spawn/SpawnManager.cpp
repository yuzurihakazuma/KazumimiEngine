#include "SpawnManager.h"

SpawnManager::SpawnManager()
	: randomEngine_(std::random_device{}()) {
}

void SpawnManager::SetLevelData(const LevelData* levelData) {
	levelData_ = levelData;
}

bool SpawnManager::HasLevelData() const {
	return levelData_ != nullptr;
}

bool SpawnManager::IsInside(int x, int z) const {
	if (!levelData_) {
		return false;
	}

	return (x >= 0 && x < levelData_->width &&
		z >= 0 && z < levelData_->height);
}

bool SpawnManager::IsFloorTile(int x, int z) const {
	if (!IsInside(x, z)) {
		return false;
	}

	return levelData_->tiles[z][x] == 0;
}

bool SpawnManager::IsWallTile(int x, int z) const {
	if (!IsInside(x, z)) {
		return false;
	}

	return levelData_->tiles[z][x] == 1;
}

bool SpawnManager::HasEnoughSpaceFromWalls(int x, int z, int margin) const {
	if (!levelData_) {
		return false;
	}

	// 中心自体が床でないなら不可
	if (!IsFloorTile(x, z)) {
		return false;
	}

	for (int dz = -margin; dz <= margin; ++dz) {
		for (int dx = -margin; dx <= margin; ++dx) {
			int checkX = x + dx;
			int checkZ = z + dz;

			// 端ギリギリを避けるため、範囲外に出るなら不可
			if (!IsInside(checkX, checkZ)) {
				return false;
			}

			// 壁があるなら不可
			if (IsWallTile(checkX, checkZ)) {
				return false;
			}
		}
	}

	return true;
}

bool SpawnManager::CanSpawnEnemy(int x, int z, int margin) const {
	if (!levelData_) {
		return false;
	}

	// 今は敵もカードも同じ基本ルール
	return HasEnoughSpaceFromWalls(x, z, margin);
}

bool SpawnManager::CanSpawnCard(int x, int z, int margin) const {
	if (!levelData_) {
		return false;
	}

	return HasEnoughSpaceFromWalls(x, z, margin);
}

std::vector<std::pair<int, int>> SpawnManager::FindEnemySpawnCandidates(int margin) const {
	std::vector<std::pair<int, int>> candidates;

	if (!levelData_) {
		return candidates;
	}

	for (int z = 0; z < levelData_->height; ++z) {
		for (int x = 0; x < levelData_->width; ++x) {
			if (CanSpawnEnemy(x, z, margin)) {
				candidates.push_back({ x, z });
			}
		}
	}

	return candidates;
}

std::vector<std::pair<int, int>> SpawnManager::FindCardSpawnCandidates(int margin) const {
	std::vector<std::pair<int, int>> candidates;

	if (!levelData_) {
		return candidates;
	}

	for (int z = 0; z < levelData_->height; ++z) {
		for (int x = 0; x < levelData_->width; ++x) {
			if (CanSpawnCard(x, z, margin)) {
				candidates.push_back({ x, z });
			}
		}
	}

	return candidates;
}

bool SpawnManager::GetRandomEnemyTile(int margin, int& outX, int& outZ) const {
	std::vector<std::pair<int, int>> candidates = FindEnemySpawnCandidates(margin);

	if (candidates.empty()) {
		return false;
	}

	std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);
	const auto& selected = candidates[dist(randomEngine_)];

	outX = selected.first;
	outZ = selected.second;
	return true;
}

bool SpawnManager::GetRandomCardTile(int margin, int& outX, int& outZ) const {
	std::vector<std::pair<int, int>> candidates = FindCardSpawnCandidates(margin);

	if (candidates.empty()) {
		return false;
	}

	std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);
	const auto& selected = candidates[dist(randomEngine_)];

	outX = selected.first;
	outZ = selected.second;
	return true;
}

Vector3 SpawnManager::TileToWorldPosition(int x, int z, float y) const {
	Vector3 worldPos{};

	if (!levelData_) {
		return worldPos;
	}

	worldPos.x = static_cast<float>(x) * levelData_->tileSize;
	worldPos.y = y;
	worldPos.z = static_cast<float>(z) * levelData_->tileSize;

	return worldPos;
}