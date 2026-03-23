#pragma once

#include <memory>
#include <vector>
#include "engine/math/VectorMath.h"
#include "engine/utils/Level/LevelData.h"

class Sprite;

class Minimap {
public:
	void Initialize();
	void SetLevelData(const LevelData* levelData);

	void SetPlayerPosition(const Vector3& worldPos);
	void SetEnemyPositions(const std::vector<Vector3>& worldPositions);

	void Update();
	void Draw();

	void SetVisible(bool visible) { visible_ = visible; }

private:
	Vector2 WorldToMinimapPosition(const Vector3& worldPos) const;
	void RebuildMapSprites();
	void EnsureEnemySprites(size_t count);

private:
	const LevelData* levelData_ = nullptr;

	bool visible_ = true;

	// 左上配置
	Vector2 mapLeftTop_ = { 20.0f, 20.0f };
	Vector2 mapSize_ = { 220.0f, 220.0f };

	float drawTileSize_ = 8.0f;

	std::unique_ptr<Sprite> backgroundSprite_;
	std::unique_ptr<Sprite> frameSprite_;

	// 固定表示は壁と階段だけ
	std::vector<std::unique_ptr<Sprite>> wallSprites_;
	std::vector<std::unique_ptr<Sprite>> stairsSprites_;

	std::unique_ptr<Sprite> playerSprite_;
	std::vector<std::unique_ptr<Sprite>> enemySprites_;

	Vector3 playerWorldPos_{};
	std::vector<Vector3> enemyWorldPositions_;
};