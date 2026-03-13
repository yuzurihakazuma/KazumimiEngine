#include "DungeonGenerator.h"
#include <algorithm>

DungeonGenerator::DungeonGenerator()
	: randomEngine_(std::random_device{}()) {
}

void DungeonGenerator::FillAll(LevelData& levelData, int tileType) {
	levelData.tiles.resize(levelData.height);

	for (int z = 0; z < levelData.height; ++z) {
		levelData.tiles[z].resize(levelData.width, tileType);
		for (int x = 0; x < levelData.width; ++x) {
			levelData.tiles[z][x] = tileType;
		}
	}
}

bool DungeonGenerator::CanPlaceRoom(const LevelData& levelData, const Room& room, int padding) const {
	int left = room.x - padding;
	int right = room.x + room.width - 1 + padding;
	int top = room.z - padding;
	int bottom = room.z + room.height - 1 + padding;

	// マップ外にはみ出すなら不可
	if (left < 0 || top < 0 || right >= levelData.width || bottom >= levelData.height) {
		return false;
	}

	// 既存部屋と重なりチェック
	for (const Room& other : rooms_) {
		int otherLeft = other.x;
		int otherRight = other.x + other.width - 1;
		int otherTop = other.z;
		int otherBottom = other.z + other.height - 1;

		bool separated =
			(right < otherLeft) ||
			(left > otherRight) ||
			(bottom < otherTop) ||
			(top > otherBottom);

		if (!separated) {
			return false;
		}
	}

	return true;
}

void DungeonGenerator::CarveRoom(LevelData& levelData, const Room& room) {
	for (int z = room.z; z < room.z + room.height; ++z) {
		for (int x = room.x; x < room.x + room.width; ++x) {
			if (x < 0 || x >= levelData.width || z < 0 || z >= levelData.height) {
				continue;
			}

			levelData.tiles[z][x] = 0;
		}
	}
}

void DungeonGenerator::GenerateRooms(LevelData& levelData, int roomCount) {
	rooms_.clear();

	// 最初に全部壁
	FillAll(levelData, 1);

	std::uniform_int_distribution<int> roomSizeDist(5, 10);

	int tryCount = 0;
	const int maxTryCount = roomCount * 30;

	while ((int)rooms_.size() < roomCount && tryCount < maxTryCount) {
		++tryCount;

		int roomWidth = roomSizeDist(randomEngine_);
		int roomHeight = roomSizeDist(randomEngine_);

		// 念のため、50x50より極端に小さい時の保険
		if (levelData.width <= roomWidth + 2 || levelData.height <= roomHeight + 2) {
			break;
		}

		std::uniform_int_distribution<int> posXDist(1, levelData.width - roomWidth - 1);
		std::uniform_int_distribution<int> posZDist(1, levelData.height - roomHeight - 1);

		Room room;
		room.x = posXDist(randomEngine_);
		room.z = posZDist(randomEngine_);
		room.width = roomWidth;
		room.height = roomHeight;

		// 1マス余白つきで置けるか
		if (!CanPlaceRoom(levelData, room, 1)) {
			continue;
		}

		CarveRoom(levelData, room);
		rooms_.push_back(room);
	}
}