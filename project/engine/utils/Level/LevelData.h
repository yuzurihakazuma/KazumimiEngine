#pragma once
#include <string>
#include <vector>
#include "engine/math/struct.h"

// レベルデータ
struct LevelObjectData{
	std::string type; // オブジェクトの種類（例: "fence", "sphere", "grass"）
	Vector3 translation; // 座標
	Vector3 rotation; // 回転
	Vector3 scale = { 1.0f,1.0f,1.0f }; // スケール
};

// マップ全体のデータ
struct LevelData{
	std::string name; // マップの名前
	std::vector<LevelObjectData> objects; // マップに配置するオブジェクトのデータ

};