#pragma once

#include "engine/utils/Level/LevelData.h"
#include "LevelManager.h"
#include <vector>
#include <memory>
#include <string>
#include "engine/math/VectorMath.h"

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
};