#pragma once // 多重インクルード防止

#include "engine/utils/Level/LevelData.h"
#include "engine/math/VectorMath.h"
#include "DungeonGenerator.h"

#include <memory>
#include <utility>

// マップ生成クラス
class MapGenerator {
public:
    // コンストラクタ
    MapGenerator();

    // デストラクタ
    ~MapGenerator();

    // 初期化処理
    void Initialize();

    // マップ全体を指定タイルで埋める
    void FillAllTiles(LevelData& levelData, int tileType);

    // 指定位置に部屋を生成
    void CreateRoom(LevelData& levelData, int startX, int startZ, int roomWidth, int roomHeight);

    // ランダムダンジョン生成
    void GenerateRandomDungeon(LevelData& levelData, int roomCount);

    // プレイヤー初期位置取得
    Vector3 GetRandomPlayerSpawnPosition(const LevelData& levelData, float y = 0.0f);

    // タイル→ワールド座標変換
    Vector3 GetTileWorldPosition(const LevelData& levelData, int x, int z, float y = 0.0f) const;

    // 階段をランダム配置（回避あり）
    void PlaceStairsTileRandom(LevelData& levelData, const Vector3& avoidWorldPos, float avoidDistance = 6.0f);

    // 階段配置＋座標取得
    std::pair<int, int> PlaceStairsTileRandomAndGetTile(LevelData& levelData, const Vector3& avoidWorldPos, float avoidDistance = 6.0f);
    const std::vector<DungeonGenerator::Room>& GetGeneratedRooms() const;

private:
    // ダンジョン生成本体
    std::unique_ptr<DungeonGenerator> dungeonGenerator_;
};