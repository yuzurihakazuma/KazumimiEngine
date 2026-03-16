#pragma once

#include "engine/utils/Level/LevelData.h"
#include "LevelManager.h"
#include <vector>
#include <memory>
#include <string>
#include "engine/math/VectorMath.h"
#include "DungeonGenerator.h"

class Obj3d;
class Camera;

// マップエディタ専用クラス
class LevelEditor {
public:
    LevelEditor();
    ~LevelEditor();

    void Initialize();
    void Update(const Vector3& playerPos);
    void Draw(const Vector3& playerPos);

    void LoadAndCreateMap(const std::string& fileName);
    void RebuildMapObjects();

    void SetCamera(const Camera* camera) { camera_ = camera; }
    void DrawDebugUI();

    const LevelData& GetLevelData() const { return levelData_; }
    void UpdateTileObject(int x, int z);
    void CreateFloorObject();
    void ResizeObjectGrids();

    void FillAllTiles(int tileType);
    void CreateRoom(int startX, int startZ, int roomWidth, int roomHeight);

    //void GenerateRandomRooms(int roomCount);

    void GenerateRandomDungeon(int roomCount);

    Vector3 GetRandomPlayerSpawnPosition(float y = 0.0f);

public:
    bool IsBossMap() const { return mapType_ == 1; }
    const std::string& GetCurrentMapFile() const { return currentMapFile_; }

    bool ConsumeMapChanged();

    Vector3 GetMapCenterPosition(float y = 0.0f) const;
private:
    const Camera* camera_ = nullptr;

    LevelData levelData_;
    // 1マスごとの床オブジェクト
    std::unique_ptr<Obj3d> floorObject_;

    // 1マスごとの壁オブジェクト
    std::vector<std::vector<std::unique_ptr<Obj3d>>> wallObjects_;

    std::string saveFileName_ = "map01.json";
    bool isEditorActive = true;

    int selectedTile_ = 0; // 0=床, 1=壁

    int editWidth_ = 10;
    int editHeight_ = 10;

    std::unique_ptr<DungeonGenerator> dungeonGenerator_;

    std::string currentMapFile_ = "resources/map/map01.json";
    int mapType_ = 0; // 0 = map01.json, 1 = boss.json

    bool mapChanged_ = false;
};