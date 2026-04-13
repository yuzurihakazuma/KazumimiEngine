#include "MapGenerator.h"

#include <random> // 乱数
#include <cmath>  // sqrt
#include <vector> // 配列

// デフォルト生成
MapGenerator::MapGenerator() = default;

// デフォルト破棄
MapGenerator::~MapGenerator() = default;

// 初期化（Generator生成）
void MapGenerator::Initialize() {
    if (!dungeonGenerator_) { // 未生成なら
        dungeonGenerator_ = std::make_unique<DungeonGenerator>(); // 作成
    }
}

// 全タイルを埋める
void MapGenerator::FillAllTiles(LevelData& levelData, int tileType) {
    for (int z = 0; z < levelData.height; ++z) { // Zループ
        for (int x = 0; x < levelData.width; ++x) { // Xループ
            levelData.tiles[z][x] = tileType; // タイル設定
        }
    }
}

// 部屋生成
void MapGenerator::CreateRoom(LevelData& levelData, int startX, int startZ, int roomWidth, int roomHeight) {
    if (roomWidth <= 0 || roomHeight <= 0) { // 無効サイズ
        return;
    }

    int endX = startX + roomWidth; // 終点X
    int endZ = startZ + roomHeight; // 終点Z

    for (int z = startZ; z < endZ; ++z) { // Z範囲
        for (int x = startX; x < endX; ++x) { // X範囲
            if (x < 0 || x >= levelData.width || z < 0 || z >= levelData.height) { // 範囲外
                continue;
            }

            levelData.tiles[z][x] = 0; // 床にする
        }
    }
}

// ダンジョン生成
void MapGenerator::GenerateRandomDungeon(LevelData& levelData, int roomCount) {
    if (!dungeonGenerator_) { // 未初期化
        Initialize();
    }

    if (levelData.width < 1 || levelData.height < 1) { // 無効サイズ
        return;
    }

    dungeonGenerator_->Generate(levelData, roomCount); // 生成実行
}

// プレイヤースポーン位置取得
Vector3 MapGenerator::GetRandomPlayerSpawnPosition(const LevelData& levelData, float y) {
    if (!dungeonGenerator_) { // Generatorなし
        return { 0.0f, levelData.baseY + y, 0.0f }; // 原点返す
    }

    return dungeonGenerator_->GetRandomRoomWorldPosition(levelData, y); // 部屋内ランダム
}

// タイル→ワールド座標
Vector3 MapGenerator::GetTileWorldPosition(const LevelData& levelData, int x, int z, float y) const {
    Vector3 pos;
    pos.x = x * levelData.tileSize; // X変換
    pos.y = levelData.baseY + y;    // Y基準
    pos.z = z * levelData.tileSize; // Z変換
    return pos;
}

// 階段ランダム配置
void MapGenerator::PlaceStairsTileRandom(LevelData& levelData, const Vector3& avoidWorldPos, float avoidDistance) {
    std::vector<std::pair<int, int>> candidates; // 候補リスト

    for (int z = 0; z < levelData.height; ++z) { // 全探索Z
        for (int x = 0; x < levelData.width; ++x) { // 全探索X
            if (levelData.tiles[z][x] != 0) { // 床以外
                continue;
            }

            Vector3 worldPos = GetTileWorldPosition(levelData, x, z, 0.0f); // 座標取得

            float dx = worldPos.x - avoidWorldPos.x; // X差
            float dz = worldPos.z - avoidWorldPos.z; // Z差
            float dist = std::sqrt(dx * dx + dz * dz); // 距離

            if (dist < avoidDistance) { // 近すぎる
                continue;
            }

            candidates.push_back({ x, z }); // 候補追加
        }
    }

    if (candidates.empty()) { // 候補なし
        for (int z = 0; z < levelData.height; ++z) {
            for (int x = 0; x < levelData.width; ++x) {
                if (levelData.tiles[z][x] == 0) { // 床のみ
                    candidates.push_back({ x, z });
                }
            }
        }
    }

    if (candidates.empty()) { // それでもなし
        return;
    }

    std::random_device rd; // シード
    std::mt19937 mt(rd()); // 乱数生成器
    std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1); // 範囲

    auto [tileX, tileZ] = candidates[dist(mt)]; // ランダム選択

    levelData.tiles[tileZ][tileX] = 3; // 階段設置
}

// 階段配置＋座標取得
std::pair<int, int> MapGenerator::PlaceStairsTileRandomAndGetTile(LevelData& levelData, const Vector3& avoidWorldPos, float avoidDistance) {
    std::vector<std::pair<int, int>> candidates; // 候補

    for (int z = 1; z < levelData.height - 1; ++z) { // 外周除外
        for (int x = 1; x < levelData.width - 1; ++x) {
            if (levelData.tiles[z][x] != 0) { // 床以外
                continue;
            }

            bool allFloor = true; // 周囲チェック
            for (int oz = -1; oz <= 1; ++oz) {
                for (int ox = -1; ox <= 1; ++ox) {
                    if (levelData.tiles[z + oz][x + ox] != 0) { // 周囲に壁
                        allFloor = false;
                        break;
                    }
                }
                if (!allFloor) {
                    break;
                }
            }

            if (!allFloor) { // 条件外
                continue;
            }

            Vector3 worldPos = GetTileWorldPosition(levelData, x, z, 0.0f); // 座標

            float dx = worldPos.x - avoidWorldPos.x; // X差
            float dz = worldPos.z - avoidWorldPos.z; // Z差
            float dist = std::sqrt(dx * dx + dz * dz); // 距離

            if (dist < avoidDistance) { // 近い
                continue;
            }

            candidates.push_back({ x, z }); // 候補追加
        }
    }

    if (candidates.empty()) { // fallback
        for (int z = 0; z < levelData.height; ++z) {
            for (int x = 0; x < levelData.width; ++x) {
                if (levelData.tiles[z][x] == 0) {
                    candidates.push_back({ x, z });
                }
            }
        }
    }

    if (candidates.empty()) { // 完全なし
        return { -1, -1 };
    }

    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist(0, static_cast<int>(candidates.size()) - 1);

    auto [tileX, tileZ] = candidates[dist(mt)];

    levelData.tiles[tileZ][tileX] = 3; // 階段設置

    return { tileX, tileZ }; // 座標返す
}

const std::vector<DungeonGenerator::Room>& MapGenerator::GetGeneratedRooms() const {
    static const std::vector<DungeonGenerator::Room> emptyRooms;
    if (!dungeonGenerator_) {
        return emptyRooms;
    }
    return dungeonGenerator_->GetRooms();
}
