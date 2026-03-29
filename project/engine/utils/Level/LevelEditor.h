#pragma once
#define NOMINMAX
#include "engine/utils/Level/LevelData.h"
#include "LevelManager.h"
#include <vector>
#include <memory>
#include <string>
#include "engine/math/VectorMath.h"
#include "DungeonGenerator.h"
#include <random>
#include "InstancedGroup.h"
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
   
    void ResizeObjectGrids();

    void FillAllTiles(int tileType);
    void CreateRoom(int startX, int startZ, int roomWidth, int roomHeight);

    //void GenerateRandomRooms(int roomCount);

    void GenerateRandomDungeon(int roomCount);

    Vector3 GetRandomPlayerSpawnPosition(float y = 0.0f);

    void ChangeToNormalMap();

    void ChangeToBossMap();
    void PlaceStairsTileRandom(const Vector3& avoidWorldPos, float avoidDistance = 6.0f);
    Vector3 GetTileWorldPosition(int x, int z, float y = 0.0f) const;
    std::pair<int, int> PlaceStairsTileRandomAndGetTile(const Vector3& avoidWorldPos, float avoidDistance = 6.0f);
public:
    bool IsBossMap() const { return mapType_ == 1; }
    const std::string& GetCurrentMapFile() const { return currentMapFile_; }

    bool ConsumeMapChanged();

    Vector3 GetMapCenterPosition(float y = 0.0f) const;

    void SetNoiseTexture(uint32_t index);

private:
    const Camera* camera_ = nullptr;

    LevelData levelData_;
    // 1マスごとの床オブジェクト
    std::vector<std::vector<std::unique_ptr<Obj3d>>> floorObjects_;

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

    std::unique_ptr<InstancedGroup> floorGroup_;
    std::unique_ptr<InstancedGroup> wallGroup_;
    std::unique_ptr<InstancedGroup> stairsGroup_;
    uint32_t noiseTextureIndex_ = 0;
};