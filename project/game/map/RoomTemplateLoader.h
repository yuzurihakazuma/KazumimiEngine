// game/map/RoomTemplateLoader.h
#pragma once

#include <string>
#include <vector>

struct RoomTemplate {
    int id = 0;
    std::string name;
    int spanX = 1;
    int spanZ = 1;
    int weight = 1;

    int enemySpawnMax = 0;
    int enemySpawnPercent = 0;
    int cardSpawnMax = 0;
    int cardSpawnPercent = 0;

    std::vector<std::vector<int>> tiles;
};


class RoomTemplateLoader {
public:
    bool LoadFromCsv(const std::string& filePath);

    const std::vector<RoomTemplate>& GetTemplates() const { return templates_; }
    bool IsLoaded() const { return !templates_.empty(); }

private:
    bool LoadTemplateTiles(const std::string& filePath, RoomTemplate& roomTemplate);

private:
    std::vector<RoomTemplate> templates_;
};
