#include "Minimap.h"
#include "Engine/2D/Sprite.h"

void Minimap::Initialize() {
	backgroundSprite_.reset(Sprite::Create("resources/white1x1.png", mapLeftTop_).release());
	backgroundSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	backgroundSprite_->SetSize(mapSize_);
	backgroundSprite_->SetColor({ 0.0f, 0.0f, 0.0f, 0.28f });
	backgroundSprite_->Update();

	frameSprite_.reset(Sprite::Create("resources/white1x1.png", { mapLeftTop_.x - 2.0f, mapLeftTop_.y - 2.0f }).release());
	frameSprite_->SetAnchorPoint({ 0.0f, 0.0f });
	frameSprite_->SetSize({ mapSize_.x + 4.0f, mapSize_.y + 4.0f });
	frameSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.18f });
	frameSprite_->Update();

	playerSprite_.reset(Sprite::Create("resources/white1x1.png", mapLeftTop_).release());
	playerSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	playerSprite_->SetSize({ 6.0f, 6.0f });
	playerSprite_->SetColor({ 1.0f, 1.0f, 0.0f, 1.0f });
	playerSprite_->Update();
}

void Minimap::SetLevelData(const LevelData* levelData) {
	levelData_ = levelData;
	RebuildMapSprites();
}

void Minimap::SetPlayerPosition(const Vector3& worldPos) {
	playerWorldPos_ = worldPos;
}

void Minimap::SetEnemyPositions(const std::vector<Vector3>& worldPositions) {
	enemyWorldPositions_ = worldPositions;
	EnsureEnemySprites(enemyWorldPositions_.size());
}

void Minimap::SetCardPositions(const std::vector<Vector3>& worldPositions) {
	cardWorldPositions_ = worldPositions;
	EnsureCardSprites(cardWorldPositions_.size());
}

void Minimap::EnsureEnemySprites(size_t count) {
	while (enemySprites_.size() < count) {
		auto sprite = std::unique_ptr<Sprite>(Sprite::Create("resources/white1x1.png", mapLeftTop_));
		sprite->SetAnchorPoint({ 0.5f, 0.5f });
		sprite->SetSize({ 5.0f, 5.0f });
		sprite->SetColor({ 1.0f, 0.3f, 0.3f, 0.95f });
		sprite->Update();
		enemySprites_.push_back(std::move(sprite));
	}

	while (enemySprites_.size() > count) {
		enemySprites_.pop_back();
	}
}

void Minimap::EnsureCardSprites(size_t count) {
	while (cardSprites_.size() < count) {
		auto sprite = std::unique_ptr<Sprite>(Sprite::Create("resources/white1x1.png", mapLeftTop_));
		sprite->SetAnchorPoint({ 0.5f, 0.5f });
		sprite->SetSize({ 4.0f, 4.0f });
		sprite->SetColor({ 0.0f, 1.0f, 0.0f, 1.0f });
		sprite->Update();
		cardSprites_.push_back(std::move(sprite));
	}

	while (cardSprites_.size() > count) {
		cardSprites_.pop_back();
	}
}

void Minimap::RebuildMapSprites() {
	wallSprites_.clear();
	stairsSprites_.clear();

	if (!levelData_) {
		return;
	}

	float tileSizeX = mapSize_.x / static_cast<float>(levelData_->width);
	float tileSizeY = mapSize_.y / static_cast<float>(levelData_->height);
	drawTileSize_ = (tileSizeX < tileSizeY) ? tileSizeX : tileSizeY;

	// 壁は横方向の連続をまとめる
	for (int z = 0; z < levelData_->height; ++z) {
		int x = 0;
		while (x < levelData_->width) {
			if (levelData_->tiles[z][x] != 1) {
				++x;
				continue;
			}

			int startX = x;
			while (x < levelData_->width && levelData_->tiles[z][x] == 1) {
				++x;
			}
			int endX = x - 1;
			int length = endX - startX + 1;

			Vector2 drawPos;
			drawPos.x = mapLeftTop_.x + startX * drawTileSize_;
			drawPos.y = mapLeftTop_.y + (levelData_->height - 1 - z) * drawTileSize_;

			auto sprite = std::unique_ptr<Sprite>(Sprite::Create("resources/white1x1.png", drawPos));
			sprite->SetAnchorPoint({ 0.0f, 0.0f });
			sprite->SetSize({ drawTileSize_ * length, drawTileSize_ });
			sprite->SetColor({ 0.78f, 0.78f, 0.88f, 0.58f });
			sprite->Update();
			wallSprites_.push_back(std::move(sprite));
		}
	}

	// 階段
	for (int z = 0; z < levelData_->height; ++z) {
		for (int x = 0; x < levelData_->width; ++x) {
			if (levelData_->tiles[z][x] != 3) {
				continue;
			}

			Vector2 drawPos;
			drawPos.x = mapLeftTop_.x + x * drawTileSize_;
			drawPos.y = mapLeftTop_.y + (levelData_->height - 1 - z) * drawTileSize_;

			auto sprite = std::unique_ptr<Sprite>(Sprite::Create("resources/white1x1.png", drawPos));
			sprite->SetAnchorPoint({ 0.0f, 0.0f });
			sprite->SetSize({ drawTileSize_, drawTileSize_ });
			sprite->SetColor({ 0.2f, 1.0f, 1.0f, 0.95f });
			sprite->Update();
			stairsSprites_.push_back(std::move(sprite));
		}
	}
}

Vector2 Minimap::WorldToMinimapPosition(const Vector3& worldPos) const {
	Vector2 result = mapLeftTop_;

	if (!levelData_) {
		return result;
	}

	float tileX = worldPos.x / levelData_->tileSize;
	float tileZ = worldPos.z / levelData_->tileSize;

	result.x = mapLeftTop_.x + (tileX + 0.5f) * drawTileSize_;
	result.y = mapLeftTop_.y + ((levelData_->height - 1 - tileZ) + 0.5f) * drawTileSize_;

	return result;
}

void Minimap::Update() {
	if (!visible_) {
		return;
	}

	if (playerSprite_) {
		playerSprite_->SetPosition(WorldToMinimapPosition(playerWorldPos_));
		playerSprite_->Update();
	}

	for (size_t i = 0; i < enemySprites_.size(); ++i) {
		if (i < enemyWorldPositions_.size()) {
			enemySprites_[i]->SetPosition(WorldToMinimapPosition(enemyWorldPositions_[i]));
			enemySprites_[i]->Update();
		} else {
			enemySprites_[i]->SetPosition({ -1000.0f, -1000.0f });
			enemySprites_[i]->Update();
		}
	}

	for (size_t i = 0; i < cardSprites_.size(); ++i) {
		if (i < cardWorldPositions_.size()) {
			cardSprites_[i]->SetPosition(WorldToMinimapPosition(cardWorldPositions_[i]));
			cardSprites_[i]->Update();
		} else {
			cardSprites_[i]->SetPosition({ -1000.0f, -1000.0f });
			cardSprites_[i]->Update();
		}
	}
}

void Minimap::Draw() {
	if (!visible_) {
		return;
	}

	if (frameSprite_) {
		frameSprite_->Draw();
	}
	if (backgroundSprite_) {
		backgroundSprite_->Draw();
	}

	for (auto& sprite : wallSprites_) {
		sprite->Draw();
	}
	for (auto& sprite : stairsSprites_) {
		sprite->Draw();
	}

	for (size_t i = 0; i < enemyWorldPositions_.size() && i < enemySprites_.size(); ++i) {
		enemySprites_[i]->Draw();
	}

	for (size_t i = 0; i < cardWorldPositions_.size() && i < cardSprites_.size(); ++i) {
		cardSprites_[i]->Draw();
	}

	if (playerSprite_) {
		playerSprite_->Draw();
	}
}