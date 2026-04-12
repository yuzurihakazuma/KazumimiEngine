#include "DungeonGenerator.h"
#include <algorithm>
#include <cmath>

DungeonGenerator::DungeonGenerator()
	: randomEngine_(std::random_device{}()) {
	templateLoader_.LoadFromCsv("resources/map/rooms/templates.csv");
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

			if (levelData.tiles[zz][x] == 1) {
				levelData.tiles[zz][x] = 0;
			}
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

			if (levelData.tiles[z][xx] == 1) {
				levelData.tiles[z][xx] = 0;
			}
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
	if (TryGenerateTemplateRooms(levelData, roomCount)) {
		return;
	}
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
	AddExtraConnections(levelData, 2, 1);
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

bool DungeonGenerator::CanPlaceTemplate(
	const std::vector<std::vector<bool>>& occupied,
	int startGX,
	int startGZ,
	const RoomTemplate& roomTemplate,
	int gridCols,
	int gridRows
) const {
	if (startGX + roomTemplate.spanX > gridCols || startGZ + roomTemplate.spanZ > gridRows) {
		return false;
	}

	for (int gz = startGZ; gz < startGZ + roomTemplate.spanZ; ++gz) {
		for (int gx = startGX; gx < startGX + roomTemplate.spanX; ++gx) {
			if (occupied[gz][gx]) {
				return false;
			}
		}
	}

	return true;
}

DungeonGenerator::Room DungeonGenerator::PlaceTemplateRoom(
	LevelData& levelData,
	const RoomTemplate& roomTemplate,
	const GridRect& area
) {
	const int availableWidth = std::max(1, area.width);
	const int availableHeight = std::max(1, area.height);

	const int templateWidth = static_cast<int>(roomTemplate.tiles.front().size());
	const int templateHeight = static_cast<int>(roomTemplate.tiles.size());

	const int offsetX = area.x + std::max(0, (availableWidth - templateWidth) / 2);
	const int offsetZ = area.z + std::max(0, (availableHeight - templateHeight) / 2);

	for (int z = 0; z < templateHeight; ++z) {
		for (int x = 0; x < templateWidth; ++x) {
			int tileX = offsetX + x;
			int tileZ = offsetZ + z;

			if (tileX <= 0 || tileX >= levelData.width - 1 || tileZ <= 0 || tileZ >= levelData.height - 1) {
				continue;
			}

			levelData.tiles[tileZ][tileX] = roomTemplate.tiles[z][x];
		}
	}

	Room room{};
	room.x = offsetX;
	room.z = offsetZ;
	room.width = templateWidth;
	room.height = templateHeight;
	return room;
}

bool DungeonGenerator::TryGenerateTemplateRooms(LevelData& levelData, int roomCount) {
	if (!templateLoader_.IsLoaded() || roomCount <= 0) {
		return false;
	}

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
		return false;
	}

	std::vector<int> cellWidths(gridCols, cellWidth);
	std::vector<int> cellHeights(gridRows, cellHeight);

	cellWidths[gridCols - 1] = innerRight - (innerLeft + (gridCols - 1) * cellWidth) + 1;
	cellHeights[gridRows - 1] = innerBottom - (innerTop + (gridRows - 1) * cellHeight) + 1;

	std::vector<int> cellXs(gridCols);
	std::vector<int> cellZs(gridRows);

	for (int gx = 0; gx < gridCols; ++gx) {
		cellXs[gx] = innerLeft + gx * cellWidth;
	}
	for (int gz = 0; gz < gridRows; ++gz) {
		cellZs[gz] = innerTop + gz * cellHeight;
	}

	std::vector<const RoomTemplate*> templates;
	for (const RoomTemplate& roomTemplate : templateLoader_.GetTemplates()) {
		templates.push_back(&roomTemplate);
	}

	if (templates.empty()) {
		return false;
	}

	std::vector<std::vector<bool>> occupied(gridRows, std::vector<bool>(gridCols, false));

	int attempts = 0;
	const int maxAttempts = roomCount * 24;

	while (static_cast<int>(rooms_.size()) < roomCount && attempts < maxAttempts) {
		++attempts;

		bool placed = false;

		std::vector<std::pair<int, int>> positions;
		for (int gz = 0; gz < gridRows; ++gz) {
			for (int gx = 0; gx < gridCols; ++gx) {
				positions.push_back({ gx, gz });
			}
		}

		std::shuffle(positions.begin(), positions.end(), randomEngine_);

		for (const auto& position : positions) {
			int startGX = position.first;
			int startGZ = position.second;

			struct Candidate {
				const RoomTemplate* roomTemplate;
				GridRect area;
			};

			std::vector<Candidate> candidates;

			for (const RoomTemplate* roomTemplate : templates) {
				if (!CanPlaceTemplate(occupied, startGX, startGZ, *roomTemplate, gridCols, gridRows)) {
					continue;
				}

				GridRect area{};
				area.x = cellXs[startGX];
				area.z = cellZs[startGZ];
				area.width = 0;
				area.height = 0;

				for (int gx = 0; gx < roomTemplate->spanX; ++gx) {
					area.width += cellWidths[startGX + gx];
				}
				for (int gz = 0; gz < roomTemplate->spanZ; ++gz) {
					area.height += cellHeights[startGZ + gz];
				}

				const int availableWidth = std::max(1, area.width);
				const int availableHeight = std::max(1, area.height);

				const int templateWidth = static_cast<int>(roomTemplate->tiles.front().size());
				const int templateHeight = static_cast<int>(roomTemplate->tiles.size());

				if (templateWidth > availableWidth || templateHeight > availableHeight) {
					continue;
				}

				candidates.push_back({ roomTemplate, area });
			}

			if (candidates.empty()) {
				continue;
			}

			int totalWeight = 0;
			for (const auto& candidate : candidates) {
				totalWeight += std::clamp(candidate.roomTemplate->weight, 1, 100);
			}

			std::uniform_int_distribution<int> dist(1, totalWeight);
			int roll = dist(randomEngine_);

			const RoomTemplate* selectedTemplate = nullptr;
			GridRect selectedArea{};

			int accumulatedWeight = 0;
			for (const auto& candidate : candidates) {
				accumulatedWeight += std::clamp(candidate.roomTemplate->weight, 1, 100);
				if (roll <= accumulatedWeight) {
					selectedTemplate = candidate.roomTemplate;
					selectedArea = candidate.area;
					break;
				}
			}

			if (!selectedTemplate) {
				continue;
			}

			rooms_.push_back(PlaceTemplateRoom(levelData, *selectedTemplate, selectedArea));

			for (int gz = startGZ; gz < startGZ + selectedTemplate->spanZ; ++gz) {
				for (int gx = startGX; gx < startGX + selectedTemplate->spanX; ++gx) {
					occupied[gz][gx] = true;
				}
			}

			placed = true;
			break;
		}

		if (!placed) {
			break;
		}
	}

	return !rooms_.empty();
}
