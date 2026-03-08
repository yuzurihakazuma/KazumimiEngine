#pragma once
#include <string>
#include <vector>

// マップ全体のデータ
struct LevelData {
	std::string name;                  // マップ名
	int width = 10;                    // 横マス数
	int height = 10;                   // 縦マス数
	float tileSize = 2.0f;             // 1マスの大きさ
	float baseY = -2.0f;               // 床の高さ
	std::vector<std::vector<int>> tiles; // 0=床, 1=壁
};