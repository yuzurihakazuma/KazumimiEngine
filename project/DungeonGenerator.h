#pragma once
#include <vector>
#include <random>
#include "engine/utils/Level/LevelData.h"

class DungeonGenerator {
public:
	struct Room {
		int x;
		int z;
		int width;
		int height;
	};

public:
	DungeonGenerator();

	// 部屋だけ生成
	void GenerateRooms(LevelData& levelData, int roomCount);

	const std::vector<Room>& GetRooms() const { return rooms_; }

private:
	void FillAll(LevelData& levelData, int tileType);
	bool CanPlaceRoom(const LevelData& levelData, const Room& room, int padding) const;
	void CarveRoom(LevelData& levelData, const Room& room);

private:
	std::vector<Room> rooms_;
	std::mt19937 randomEngine_;
};