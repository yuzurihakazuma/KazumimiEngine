#pragma once

#include "engine/utils/Level/LevelData.h"
#include "LevelManager.h"
#include <vector>
#include <memory>
#include <string>

class Obj3d;
class Camera;

// マップエディタ専用クラス
class LevelEditor {
public:
    LevelEditor();
    ~LevelEditor();

    void Initialize();
    void Update();
    void Draw();

    void LoadAndCreateMap(const std::string& fileName);
    void RebuildMapObjects();

    void SetCamera(const Camera* camera) { camera_ = camera; }
    void DrawDebugUI();

    const LevelData& GetLevelData() const { return levelData_; }

private:
    const Camera* camera_ = nullptr;

    LevelData levelData_;
    std::vector<std::unique_ptr<Obj3d>> object3ds_;

    std::string saveFileName_ = "map01.json";
    bool isEditorActive = true;

    int selectedTile_ = 0; // 0=床, 1=壁
};