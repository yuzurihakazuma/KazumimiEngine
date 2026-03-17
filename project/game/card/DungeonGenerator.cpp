#include "DungeonGenerator.h"
#include <algorithm>
#include <cmath>

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

void DungeonGenerator::CarveRoom(LevelData& levelData, const Room& room) {
	for (int z = room.z; z < room.z + room.height; ++z) {
		for (int x = room.x; x < room.x + room.width; ++x) {
			if (x <= 0 || x >= levelData.width - 1 || z <= 0 || z >= levelData.height - 1) {
				continue;
			}
			levelData.tiles[z][x] = 0;
		}
	}
}

void DungeonGenerator::CarveHorizontalCorridor(LevelData& levelData, int x1, int x2, int z, int width) {
	if (x1 > x2) {
		std::swap(x1, x2);
	}

	for (int x = x1; x <= x2; ++x) {
		for (int offset = 0; offset < width; ++offset) {
			int zz = z + offset;

			if (x <= 0 || x >= levelData.width - 1 || zz <= 0 || zz >= levelData.height - 1) {
				continue;
			}

			levelData.tiles[zz][x] = 0;
		}
	}
}

void DungeonGenerator::CarveVerticalCorridor(LevelData& levelData, int z1, int z2, int x, int width) {
	if (z1 > z2) {
		std::swap(z1, z2);
	}

	for (int z = z1; z <= z2; ++z) {
		for (int offset = 0; offset < width; ++offset) {
			int xx = x + offset;

			if (xx <= 0 || xx >= levelData.width - 1 || z <= 0 || z >= levelData.height - 1) {
				continue;
			}

			levelData.tiles[z][xx] = 0;
		}
	}
}

Vector2 DungeonGenerator::GetRoomCenter(const Room& room) const {
	Vector2 center{};
	center.x = static_cast<float>(room.x + room.width / 2);
	center.y = static_cast<float>(room.z + room.height / 2);
	return center;
}

void DungeonGenerator::GenerateGridRooms(LevelData& levelData, int roomCount) {
	rooms_.clear();
	FillAll(levelData, 1);

	// ポケダン風に区画分割
	const int gridCols = 4;
	const int gridRows = 4;

	const int innerLeft = 1;
	const int innerTop = 1;
	const int innerRight = levelData.width - 2;
	const int innerBottom = levelData.height - 2;

	const int usableWidth = innerRight - innerLeft + 1;
	const int usableHeight = innerBottom - innerTop + 1;

	const int cellWidth = usableWidth / gridCols;
	const int cellHeight = usableHeight / gridRows;

	if (cellWidth < 6 || cellHeight < 6) {
		return;
	}

	struct CellRoom {
		bool hasRoom;
		Room room;
		int gx;
		int gz;
	};

	std::vector<CellRoom> candidates;

	for (int gz = 0; gz < gridRows; ++gz) {
		for (int gx = 0; gx < gridCols; ++gx) {
			int cellX = innerLeft + gx * cellWidth;
			int cellZ = innerTop + gz * cellHeight;

			int currentCellWidth = (gx == gridCols - 1) ? (innerRight - cellX + 1) : cellWidth;
			int currentCellHeight = (gz == gridRows - 1) ? (innerBottom - cellZ + 1) : cellHeight;

			int minRoomW = 10;
			int minRoomH = 10;

			int maxRoomW = std::max(minRoomW, currentCellWidth - 2);
			int maxRoomH = std::max(minRoomH, currentCellHeight - 2);

			if (maxRoomW < minRoomW || maxRoomH < minRoomH) {
				continue;
			}

			std::uniform_int_distribution<int> roomWDist(minRoomW, std::min(maxRoomW, 16));
			std::uniform_int_distribution<int> roomHDist(minRoomH, std::min(maxRoomH, 16));

			int roomW = roomWDist(randomEngine_);
			int roomH = roomHDist(randomEngine_);

			int minX = cellX + 1;
			int maxX = cellX + currentCellWidth - roomW - 1;
			int minZ = cellZ + 1;
			int maxZ = cellZ + currentCellHeight - roomH - 1;

			if (minX > maxX || minZ > maxZ) {
				continue;
			}

			std::uniform_int_distribution<int> roomXDist(minX, maxX);
			std::uniform_int_distribution<int> roomZDist(minZ, maxZ);

			Room room{};
			room.x = roomXDist(randomEngine_);
			room.z = roomZDist(randomEngine_);
			room.width = roomW;
			room.height = roomH;

			CellRoom cellRoom{};
			cellRoom.hasRoom = true;
			cellRoom.room = room;
			cellRoom.gx = gx;
			cellRoom.gz = gz;

			candidates.push_back(cellRoom);
		}
	}

	if (candidates.empty()) {
		return;
	}

	std::shuffle(candidates.begin(), candidates.end(), randomEngine_);

	int createCount = std::min(roomCount, static_cast<int>(candidates.size()));

	for (int i = 0; i < createCount; ++i) {
		rooms_.push_back(candidates[i].room);
		CarveRoom(levelData, candidates[i].room);
	}
}

void DungeonGenerator::ConnectRooms(LevelData& levelData, const Room& a, const Room& b, int corridorWidth) {
	Vector2 centerA = GetRoomCenter(a);
	Vector2 centerB = GetRoomCenter(b);

	int ax = static_cast<int>(centerA.x);
	int az = static_cast<int>(centerA.y);
	int bx = static_cast<int>(centerB.x);
	int bz = static_cast<int>(centerB.y);

	// ポケダンっぽく素直なL字
	std::uniform_int_distribution<int> orderDist(0, 1);
	int order = orderDist(randomEngine_);

	if (order == 0) {
		CarveHorizontalCorridor(levelData, ax, bx, az, corridorWidth);
		CarveVerticalCorridor(levelData, az, bz, bx, corridorWidth);
	} else {
		CarveVerticalCorridor(levelData, az, bz, ax, corridorWidth);
		CarveHorizontalCorridor(levelData, ax, bx, bz, corridorWidth);
	}
}

void DungeonGenerator::ConnectAllRooms(LevelData& levelData, int corridorWidth) {
	if (rooms_.size() < 2) {
		return;
	}

	// 全部屋を左上寄り順に並べる
	std::vector<Room> sortedRooms = rooms_;
	std::sort(sortedRooms.begin(), sortedRooms.end(),
		[](const Room& a, const Room& b) {
			if (a.z == b.z) {
				return a.x < b.x;
			}
			return a.z < b.z;
		});

	// まず最低限つなぐ
	for (size_t i = 0; i + 1 < sortedRooms.size(); ++i) {
		ConnectRooms(levelData, sortedRooms[i], sortedRooms[i + 1], corridorWidth);
	}
}

void DungeonGenerator::AddExtraConnections(LevelData& levelData, int corridorWidth, int count) {
	if (rooms_.size() < 3) {
		return;
	}

	std::uniform_int_distribution<int> roomDist(0, static_cast<int>(rooms_.size()) - 1);

	for (int i = 0; i < count; ++i) {
		int a = roomDist(randomEngine_);
		int b = roomDist(randomEngine_);

		if (a == b) {
			continue;
		}

		ConnectRooms(levelData, rooms_[a], rooms_[b], corridorWidth);
	}
}

void DungeonGenerator::Generate(LevelData& levelData, int roomCount) {
	GenerateGridRooms(levelData, roomCount);

	// 通路幅は2マスのまま
	ConnectAllRooms(levelData, 2);

	// 少しだけ余分な接続を足してポケダンっぽい枝や交差を増やす
	AddExtraConnections(levelData, 2, 3);
}

void DungeonGenerator::GenerateRooms(LevelData& levelData, int roomCount) {
	// 外からは今まで通り呼べるようにしておく
	Generate(levelData, roomCount);
}

Vector3 DungeonGenerator::GetRandomRoomWorldPosition(const LevelData& levelData, float y) {
	if (rooms_.empty()) {
		return { 0.0f, levelData.baseY + 1.0f + y, 0.0f };
	}

	std::uniform_int_distribution<int> roomDist(0, static_cast<int>(rooms_.size()) - 1);
	const Room& room = rooms_[roomDist(randomEngine_)];

	int minX = room.x + 1;
	int maxX = room.x + room.width - 2;
	int minZ = room.z + 1;
	int maxZ = room.z + room.height - 2;

	// 小部屋対策
	if (minX > maxX) {
		minX = room.x;
		maxX = room.x + room.width - 1;
	}
	if (minZ > maxZ) {
		minZ = room.z;
		maxZ = room.z + room.height - 1;
	}

	std::uniform_int_distribution<int> xDist(minX, maxX);
	std::uniform_int_distribution<int> zDist(minZ, maxZ);

	int tileX = xDist(randomEngine_);
	int tileZ = zDist(randomEngine_);

	Vector3 worldPos;
	worldPos.x = tileX * levelData.tileSize;
	worldPos.y = levelData.baseY + 1.0f + y;
	worldPos.z = tileZ * levelData.tileSize;

	return worldPos;
}