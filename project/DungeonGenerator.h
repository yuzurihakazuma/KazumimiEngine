#pragma once
#include <vector>
#include <random>
#include "engine/utils/Level/LevelData.h"
#include "engine/math/VectorMath.h"
#include "game/map/RoomTemplateLoader.h"

class DungeonGenerator {
public:
	struct Room {
		int x = 0;
		int z = 0;
		int width = 0;
		int height = 0;
		int spanX = 1;
		int spanZ = 1;

		int templateId = -1;
		std::string templateName;

		int enemySpawnMax = 0;
		int enemySpawnPercent = 0;
		int cardSpawnMax = 0;
		int cardSpawnPercent = 0;
	};


	struct CorridorPoint {
		int x;
		int z;
	};
private:
	struct GridRect {
		int x;
		int z;
		int width;
		int height;
	};

	bool TryGenerateTemplateRooms(LevelData& levelData, int roomCount);
	bool CanPlaceTemplate(
		const std::vector<std::vector<bool>>& occupied,
		int startGX,
		int startGZ,
		const RoomTemplate& roomTemplate,
		int gridCols,
		int gridRows
	) const;
	Room PlaceTemplateRoom(LevelData& levelData, const RoomTemplate& roomTemplate, const GridRect& area);

public:
	DungeonGenerator();

	// 外からは今まで通りこれを呼べばOK
	void Generate(LevelData& levelData, int roomCount);
	void GenerateRooms(LevelData& levelData, int roomCount);

	const std::vector<Room>& GetRooms() const { return rooms_; }

	Vector3 GetRandomRoomWorldPosition(const LevelData& levelData, float y = 0.0f);

private:
	void FillAll(LevelData& levelData, int tileType);
	void CarveRoom(LevelData& levelData, const Room& room);
	void CarveHorizontalCorridor(LevelData& levelData, int x1, int x2, int z, int width);
	void CarveVerticalCorridor(LevelData& levelData, int z1, int z2, int x, int width);

	void GenerateGridRooms(LevelData& levelData, int roomCount);
	void ConnectRooms(LevelData& levelData, const Room& a, const Room& b, int corridorWidth);
	void ConnectAllRooms(LevelData& levelData, int corridorWidth);
	void AddExtraConnections(LevelData& levelData, int corridorWidth, int count);

	Vector2 GetRoomCenter(const Room& room) const;
	CorridorPoint GetRoomConnectionPoint(const Room& room, const Room& targetRoom) const;

private:
	std::vector<Room> rooms_;
	std::mt19937 randomEngine_;
	RoomTemplateLoader templateLoader_;
};
