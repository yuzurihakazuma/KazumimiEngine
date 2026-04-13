// game/map/RoomTemplateLoader.cpp
#include "RoomTemplateLoader.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace {
    std::vector<std::string> SplitCsvLine(const std::string& line) {
        std::vector<std::string> columns;
        std::stringstream ss(line);
        std::string token;

        while (std::getline(ss, token, ',')) {
            columns.push_back(token);
        }

        return columns;
    }
}

bool RoomTemplateLoader::LoadFromCsv(const std::string& filePath) {
    templates_.clear();

    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    bool isFirstLine = true;

    while (std::getline(file, line)) {
        if (isFirstLine) {
            isFirstLine = false;
            continue;
        }

        if (line.empty()) {
            continue;
        }

        std::vector<std::string> columns = SplitCsvLine(line);
        if (columns.size() < 10) {
            continue;
        }

        RoomTemplate roomTemplate;
        roomTemplate.id = std::stoi(columns[0]);
        roomTemplate.name = columns[1];
        roomTemplate.spanX = std::max(1, std::stoi(columns[2]));
        roomTemplate.spanZ = std::max(1, std::stoi(columns[3]));
        roomTemplate.weight = std::max(1, std::stoi(columns[5]));

        roomTemplate.enemySpawnMax = std::max(0, std::stoi(columns[6]));
        roomTemplate.enemySpawnPercent = std::clamp(std::stoi(columns[7]), 0, 100);
        roomTemplate.cardSpawnMax = std::max(0, std::stoi(columns[8]));
        roomTemplate.cardSpawnPercent = std::clamp(std::stoi(columns[9]), 0, 100);

        if (!LoadTemplateTiles(columns[4], roomTemplate)) {
            continue;
        }

        templates_.push_back(roomTemplate);
    }

    return !templates_.empty();
}


bool RoomTemplateLoader::LoadTemplateTiles(const std::string& filePath, RoomTemplate& roomTemplate) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    roomTemplate.tiles.clear();
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::vector<std::string> columns = SplitCsvLine(line);
        std::vector<int> row;

        for (const std::string& column : columns) {
            row.push_back(std::stoi(column));
        }

        if (!row.empty()) {
            roomTemplate.tiles.push_back(row);
        }
    }

    if (roomTemplate.tiles.empty()) {
        return false;
    }

    const size_t width = roomTemplate.tiles.front().size();
    for (const auto& row : roomTemplate.tiles) {
        if (row.size() != width) {
            return false;
        }
    }

    return true;
}
