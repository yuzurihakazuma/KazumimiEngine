#pragma once

#define NOMINMAX
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
	void SetCardPositions(const std::vector<Vector3>& worldPositions); // 追加

	void Update();
	void Draw();

	void SetVisible(bool visible) { visible_ = visible; }

private:
	Vector2 WorldToMinimapPosition(const Vector3& worldPos) const;
	void EnsureEnemySprites(size_t count);
	void EnsureCardSprites(size_t count); // 追加

	void RebuildMapSprites();

private:
	const LevelData* levelData_ = nullptr;

	bool visible_ = true;

	Vector2 mapLeftTop_ = { 20.0f, 20.0f };
	Vector2 mapSize_ = { 220.0f, 220.0f };

	float drawTileSize_ = 8.0f;

	std::unique_ptr<Sprite> backgroundSprite_;
	std::unique_ptr<Sprite> frameSprite_;

	std::unique_ptr<Sprite> playerSprite_;
	std::vector<std::unique_ptr<Sprite>> enemySprites_;
	std::vector<std::unique_ptr<Sprite>> cardSprites_; // 追加

	Vector3 playerWorldPos_{};
	std::vector<Vector3> enemyWorldPositions_;
	std::vector<Vector3> cardWorldPositions_; // 追加

	std::vector<std::unique_ptr<Sprite>> wallSprites_;
	std::vector<std::unique_ptr<Sprite>> stairsSprites_;
};