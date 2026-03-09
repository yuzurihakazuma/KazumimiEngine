#include "LevelManager.h"
#include "externals/nlohmann/json.hpp"
#include <fstream>

using json = nlohmann::json;

void LevelManager::Save(const std::string& fileName, const LevelData& levelData) {
    json j;
    j["name"] = levelData.name;
    j["width"] = levelData.width;
    j["height"] = levelData.height;
    j["tileSize"] = levelData.tileSize;
    j["baseY"] = levelData.baseY;
    j["tiles"] = levelData.tiles;

    std::ofstream file(fileName);
    if (file.is_open()) {
        file << j.dump(4);
        file.close();
    }
}

LevelData LevelManager::Load(const std::string& fileName) {
    LevelData levelData;

    std::ifstream file(fileName);
    if (!file.is_open()) {
        // デフォルトで10x10の床マップを作る
        levelData.name = "NewMap";
        levelData.width = 10;
        levelData.height = 10;
        levelData.tileSize = 2.0f;
        levelData.baseY = -2.0f;
        levelData.tiles.resize(levelData.height, std::vector<int>(levelData.width, 0));
        return levelData;
    }

    json j;
    file >> j;

    levelData.name = j.value("name", "Unknown");
    levelData.width = j.value("width", 10);
    levelData.height = j.value("height", 10);
    levelData.tileSize = j.value("tileSize", 2.0f);
    levelData.baseY = j.value("baseY", -2.0f);

    if (j.contains("tiles") && j["tiles"].is_array()) {
        levelData.tiles = j["tiles"].get<std::vector<std::vector<int>>>();
    } else {
        levelData.tiles.resize(levelData.height, std::vector<int>(levelData.width, 0));
    }

    if (!levelData.tiles.empty()) {
        levelData.height = static_cast<int>(levelData.tiles.size());
        levelData.width = static_cast<int>(levelData.tiles[0].size());
    }

    // 念のためサイズ補正
    if ((int)levelData.tiles.size() != levelData.height) {
        levelData.tiles.resize(levelData.height, std::vector<int>(levelData.width, 0));
    }
    for (auto& row : levelData.tiles) {
        if ((int)row.size() != levelData.width) {
            row.resize(levelData.width, 0);
        }
    }

    return levelData;
}