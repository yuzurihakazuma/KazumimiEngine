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

	const int minOffset = -(width / 2);
	const int maxOffset = minOffset + width - 1;

	for (int x = x1; x <= x2; ++x) {
		for (int offset = minOffset; offset <= maxOffset; ++offset) {
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

	const int minOffset = -(width / 2);
	const int maxOffset = minOffset + width - 1;

	for (int z = z1; z <= z2; ++z) {
		for (int offset = minOffset; offset <= maxOffset; ++offset) {
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

DungeonGenerator::CorridorPoint DungeonGenerator::GetRoomConnectionPoint(const Room& room, const Room& targetRoom) const {
	CorridorPoint point{};

	const int splitX = std::max(1, room.spanX);
	const int splitZ = std::max(1, room.spanZ);

	const Vector2 roomCenter = GetRoomCenter(room);
	const Vector2 targetCenter = GetRoomCenter(targetRoom);

	const int segmentX = (targetCenter.x < roomCenter.x) ? 0 : (splitX - 1);
	const int segmentZ = (targetCenter.y < roomCenter.y) ? 0 : (splitZ - 1);

	const int startX = room.x + (room.width * segmentX) / splitX;
	const int endX = room.x + (room.width * (segmentX + 1)) / splitX - 1;
	const int startZ = room.z + (room.height * segmentZ) / splitZ;
	const int endZ = room.z + (room.height * (segmentZ + 1)) / splitZ - 1;

	point.x = (startX + endX) / 2;
	point.z = (startZ + endZ) / 2;

	return point;
}

DungeonGenerator::CorridorPoint DungeonGenerator::GetGridCellCenter(const LevelData& levelData, int gx, int gz) const {
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

	const int cellX = innerLeft + gx * cellWidth;
	const int cellZ = innerTop + gz * cellHeight;

	const int currentCellWidth = (gx == gridCols - 1) ? (innerRight - cellX + 1) : cellWidth;
	const int currentCellHeight = (gz == gridRows - 1) ? (innerBottom - cellZ + 1) : cellHeight;

	CorridorPoint point{};
	point.x = cellX + currentCellWidth / 2;
	point.z = cellZ + currentCellHeight / 2;
	return point;
}

DungeonGenerator::CorridorPoint DungeonGenerator::GetRoomGridAnchor(const LevelData& levelData, const Room& room) const {
	const int anchorGX = room.gridX + (std::max(1, room.spanX) - 1) / 2;
	const int anchorGZ = room.gridZ + (std::max(1, room.spanZ) - 1) / 2;
	return GetGridCellCenter(levelData, anchorGX, anchorGZ);
}

void DungeonGenerator::CarveGridPath(
	LevelData& levelData,
	const CorridorPoint& start,
	const CorridorPoint& end,
	int startGX,
	int startGZ,
	int endGX,
	int endGZ,
	int corridorWidth
) {
	int currentGX = startGX;
	int currentGZ = startGZ;
	CorridorPoint currentPoint = start;

	std::uniform_int_distribution<int> orderDist(0, 1);
	const bool horizontalFirst = orderDist(randomEngine_) == 0;

	auto stepHorizontal = [&](int targetGX) {
		while (currentGX != targetGX) {
			const int nextGX = currentGX + ((targetGX > currentGX) ? 1 : -1);
			const CorridorPoint nextPoint = GetGridCellCenter(levelData, nextGX, currentGZ);
			CarveHorizontalCorridor(levelData, currentPoint.x, nextPoint.x, currentPoint.z, corridorWidth);
			currentGX = nextGX;
			currentPoint = nextPoint;
		}
	};

	auto stepVertical = [&](int targetGZ) {
		while (currentGZ != targetGZ) {
			const int nextGZ = currentGZ + ((targetGZ > currentGZ) ? 1 : -1);
			const CorridorPoint nextPoint = GetGridCellCenter(levelData, currentGX, nextGZ);
			CarveVerticalCorridor(levelData, currentPoint.z, nextPoint.z, currentPoint.x, corridorWidth);
			currentGZ = nextGZ;
			currentPoint = nextPoint;
		}
	};

	if (horizontalFirst) {
		stepHorizontal(endGX);
		stepVertical(endGZ);
	} else {
		stepVertical(endGZ);
		stepHorizontal(endGX);
	}

	if (currentPoint.x != end.x) {
		CarveHorizontalCorridor(levelData, currentPoint.x, end.x, currentPoint.z, corridorWidth);
		currentPoint.x = end.x;
	}
	if (currentPoint.z != end.z) {
		CarveVerticalCorridor(levelData, currentPoint.z, end.z, currentPoint.x, corridorWidth);
	}
}

void DungeonGenerator::GenerateGridRooms(LevelData& levelData, int roomCount) {
	rooms_.clear();
	FillAll(levelData, 1);
	if (TryGenerateTemplateRooms(levelData, roomCount)) {
		return;
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
		return;
	}

	struct CellRoom {
		Room room;
	};

	std::vector<CellRoom> candidates;

	for (int gz = 0; gz < gridRows; ++gz) {
		for (int gx = 0; gx < gridCols; ++gx) {
			const int cellX = innerLeft + gx * cellWidth;
			const int cellZ = innerTop + gz * cellHeight;

			const int currentCellWidth = (gx == gridCols - 1) ? (innerRight - cellX + 1) : cellWidth;
			const int currentCellHeight = (gz == gridRows - 1) ? (innerBottom - cellZ + 1) : cellHeight;

			const int minRoomW = 10;
			const int minRoomH = 10;

			const int maxRoomW = std::max(minRoomW, currentCellWidth - 2);
			const int maxRoomH = std::max(minRoomH, currentCellHeight - 2);

			if (maxRoomW < minRoomW || maxRoomH < minRoomH) {
				continue;
			}

			std::uniform_int_distribution<int> roomWDist(minRoomW, std::min(maxRoomW, 16));
			std::uniform_int_distribution<int> roomHDist(minRoomH, std::min(maxRoomH, 16));

			const int roomW = roomWDist(randomEngine_);
			const int roomH = roomHDist(randomEngine_);

			const int minX = cellX + 1;
			const int maxX = cellX + currentCellWidth - roomW - 1;
			const int minZ = cellZ + 1;
			const int maxZ = cellZ + currentCellHeight - roomH - 1;

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
			room.gridX = gx;
			room.gridZ = gz;

			candidates.push_back({ room });
		}
	}

	if (candidates.empty()) {
		return;
	}

	std::shuffle(candidates.begin(), candidates.end(), randomEngine_);

	const int createCount = std::min(roomCount, static_cast<int>(candidates.size()));

	for (int i = 0; i < createCount; ++i) {
		rooms_.push_back(candidates[i].room);
		CarveRoom(levelData, candidates[i].room);
	}
}

void DungeonGenerator::ConnectRooms(LevelData& levelData, const Room& a, const Room& b, int corridorWidth) {
	const CorridorPoint pointA = GetRoomConnectionPoint(a, b);
	const CorridorPoint pointB = GetRoomConnectionPoint(b, a);
	const CorridorPoint anchorA = GetRoomGridAnchor(levelData, a);
	const CorridorPoint anchorB = GetRoomGridAnchor(levelData, b);

	if (pointA.x != anchorA.x) {
		CarveHorizontalCorridor(levelData, pointA.x, anchorA.x, pointA.z, corridorWidth);
	}
	if (pointA.z != anchorA.z) {
		CarveVerticalCorridor(levelData, pointA.z, anchorA.z, anchorA.x, corridorWidth);
	}

	CarveGridPath(
		levelData,
		anchorA,
		anchorB,
		a.gridX + (std::max(1, a.spanX) - 1) / 2,
		a.gridZ + (std::max(1, a.spanZ) - 1) / 2,
		b.gridX + (std::max(1, b.spanX) - 1) / 2,
		b.gridZ + (std::max(1, b.spanZ) - 1) / 2,
		corridorWidth
	);

	if (anchorB.z != pointB.z) {
		CarveVerticalCorridor(levelData, anchorB.z, pointB.z, anchorB.x, corridorWidth);
	}
	if (anchorB.x != pointB.x) {
		CarveHorizontalCorridor(levelData, anchorB.x, pointB.x, pointB.z, corridorWidth);
	}
}

void DungeonGenerator::ConnectAllRooms(LevelData& levelData, int corridorWidth) {
	if (rooms_.size() < 2) {
		return;
	}

	struct Edge {
		int a;
		int b;
		int cost;
	};

	std::vector<Edge> edges;

	for (int i = 0; i < static_cast<int>(rooms_.size()); ++i) {
		for (int j = i + 1; j < static_cast<int>(rooms_.size()); ++j) {
			const int ax = rooms_[i].gridX + (std::max(1, rooms_[i].spanX) - 1) / 2;
			const int az = rooms_[i].gridZ + (std::max(1, rooms_[i].spanZ) - 1) / 2;
			const int bx = rooms_[j].gridX + (std::max(1, rooms_[j].spanX) - 1) / 2;
			const int bz = rooms_[j].gridZ + (std::max(1, rooms_[j].spanZ) - 1) / 2;

			const int cost = std::abs(ax - bx) + std::abs(az - bz);
			edges.push_back({ i, j, cost });
		}
	}

	std::sort(edges.begin(), edges.end(), [](const Edge& l, const Edge& r) {
		return l.cost < r.cost;
		});

	struct UnionFind {
		std::vector<int> parent;
		std::vector<int> size;

		explicit UnionFind(int n) : parent(n), size(n, 1) {
			for (int i = 0; i < n; ++i) {
				parent[i] = i;
			}
		}

		int Find(int x) {
			if (parent[x] == x) {
				return x;
			}
			parent[x] = Find(parent[x]);
			return parent[x];
		}

		bool Unite(int a, int b) {
			a = Find(a);
			b = Find(b);
			if (a == b) {
				return false;
			}
			if (size[a] < size[b]) {
				std::swap(a, b);
			}
			parent[b] = a;
			size[a] += size[b];
			return true;
		}
	};

	UnionFind uf(static_cast<int>(rooms_.size()));

	int connectedCount = 0;
	for (const Edge& edge : edges) {
		if (uf.Unite(edge.a, edge.b)) {
			ConnectRooms(levelData, rooms_[edge.a], rooms_[edge.b], corridorWidth);
			++connectedCount;
			if (connectedCount >= static_cast<int>(rooms_.size()) - 1) {
				break;
			}
		}
	}

	int extraCount = 0;
	for (const Edge& edge : edges) {
		if (edge.cost > 2) {
			continue;
		}

		std::uniform_int_distribution<int> extraDist(0, 99);
		if (extraDist(randomEngine_) < 35) {
			ConnectRooms(levelData, rooms_[edge.a], rooms_[edge.b], corridorWidth);
			++extraCount;
		}

		if (extraCount >= 4) {
			break;
		}
	}
}


void DungeonGenerator::AddExtraConnections(LevelData& levelData, int corridorWidth, int count) {
	if (rooms_.size() < 3) {
		return;
	}

	std::uniform_int_distribution<int> roomDist(0, static_cast<int>(rooms_.size()) - 1);

	for (int i = 0; i < count; ++i) {
		const int a = roomDist(randomEngine_);
		const int b = roomDist(randomEngine_);

		if (a == b) {
			continue;
		}

		ConnectRooms(levelData, rooms_[a], rooms_[b], corridorWidth);
	}
}

void DungeonGenerator::Generate(LevelData& levelData, int roomCount) {
	GenerateGridRooms(levelData, roomCount);

	ConnectAllRooms(levelData, 2);
	AddExtraConnections(levelData, 2, 5);
}

void DungeonGenerator::GenerateRooms(LevelData& levelData, int roomCount) {
	Generate(levelData, roomCount);
}

Vector3 DungeonGenerator::GetRandomRoomWorldPosition(const LevelData& levelData, float y) {
	if (rooms_.empty()) {
		return { 0.0f, levelData.baseY + 1.0f + y, 0.0f };
	}

	std::uniform_int_distribution<int> roomDist(0, static_cast<int>(rooms_.size()) - 1);
	const Room& room = rooms_[roomDist(randomEngine_)];

	std::vector<std::pair<int, int>> candidates;

	const int minX = room.x + 1;
	const int maxX = room.x + room.width - 2;
	const int minZ = room.z + 1;
	const int maxZ = room.z + room.height - 2;

	for (int z = minZ; z <= maxZ; ++z) {
		for (int x = minX; x <= maxX; ++x) {
			if (x < 0 || x >= levelData.width || z < 0 || z >= levelData.height) {
				continue;
			}

			if (levelData.tiles[z][x] == 0) {
				candidates.push_back({ x, z });
			}
		}
	}

	if (candidates.empty()) {
		for (int z = room.z; z < room.z + room.height; ++z) {
			for (int x = room.x; x < room.x + room.width; ++x) {
				if (x < 0 || x >= levelData.width || z < 0 || z >= levelData.height) {
					continue;
				}

				if (levelData.tiles[z][x] == 0) {
					candidates.push_back({ x, z });
				}
			}
		}
	}

	if (candidates.empty()) {
		return { 0.0f, levelData.baseY + 1.0f + y, 0.0f };
	}

	std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);
	auto [tileX, tileZ] = candidates[dist(randomEngine_)];

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
	const GridRect& area,
	int startGX,
	int startGZ
) {
	const int availableWidth = std::max(1, area.width);
	const int availableHeight = std::max(1, area.height);

	const int templateWidth = static_cast<int>(roomTemplate.tiles.front().size());
	const int templateHeight = static_cast<int>(roomTemplate.tiles.size());

	const int offsetX = area.x + std::max(0, (availableWidth - templateWidth) / 2);
	const int offsetZ = area.z + std::max(0, (availableHeight - templateHeight) / 2);

	for (int z = 0; z < templateHeight; ++z) {
		for (int x = 0; x < templateWidth; ++x) {
			const int tileX = offsetX + x;
			const int tileZ = offsetZ + z;

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
	room.gridX = startGX;
	room.gridZ = startGZ;
	room.spanX = std::max(1, roomTemplate.spanX);
	room.spanZ = std::max(1, roomTemplate.spanZ);
	room.templateId = roomTemplate.id;
	room.templateName = roomTemplate.name;
	room.enemySpawnMax = roomTemplate.enemySpawnMax;
	room.enemySpawnPercent = roomTemplate.enemySpawnPercent;
	room.cardSpawnMax = roomTemplate.cardSpawnMax;
	room.cardSpawnPercent = roomTemplate.cardSpawnPercent;

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
			const int startGX = position.first;
			const int startGZ = position.second;

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
			const int roll = dist(randomEngine_);

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

			rooms_.push_back(PlaceTemplateRoom(levelData, *selectedTemplate, selectedArea, startGX, startGZ));

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
