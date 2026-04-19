// Tutorial.cpp
#include "Tutorial.h"

#include <cmath>

#include "engine/utils/TextManager.h"
#include "game/map/MapManager.h"
#include "game/player/PlayerManager.h"
#include "game/enemy/EnemyManager.h"
#include "game/card/CardPickupManager.h"
#include "game/card/CardDatabase.h"
#include "game/map/Minimap.h"
#include "engine/camera/Camera.h"
#include "engine/utils/Level/LevelData.h"

void Tutorial::Initialize(const Context& context) {
	context_ = context;
}

void Tutorial::Start() {
	if (!context_.mapManager || !context_.playerManager || !context_.cardPickupManager) {
		return;
	}

	isActive_ = true;
	requestReturnToTitle_ = false;

	pickupSpawned_ = false;
	enemySpawned_ = false;
	stairsSpawned_ = false;
	stairsX_ = -1;
	stairsZ_ = -1;
	step_ = Step::PickCard;

	ClearGameplayObjects();
	BuildTutorialMap();

	// 部屋中央に置くとカードと重なるので少し左にずらす
	PlacePlayerAt(room1_.left + 2, room1_.CenterZ());

	SpawnTutorialCard();
	UpdateTexts();
}

void Tutorial::Update() {
	if (!isActive_) {
		return;
	}

	if (step_ == Step::PickCard && pickupSpawned_ && AreAllPickupsCollected()) {
		OpenCorridor(corridor1_);
		SpawnTutorialEnemy();
		step_ = Step::DefeatEnemy;
		UpdateTexts();
		return;
	}

	if (step_ == Step::DefeatEnemy && enemySpawned_ && AreAllEnemiesDefeated()) {
		OpenCorridor(corridor2_);
		SpawnTutorialStairs();
		step_ = Step::ReachStairs;
		UpdateTexts();
	}
}

void Tutorial::Finalize() {
	ClearTexts();
	isActive_ = false;
	requestReturnToTitle_ = false;
}

bool Tutorial::ConsumeReturnToTitleRequest() {
	const bool requested = requestReturnToTitle_;
	requestReturnToTitle_ = false;
	return requested;
}

void Tutorial::CheckPlayerGoal(const Vector3& playerWorldPos) {
	if (!isActive_ || step_ != Step::ReachStairs || !stairsSpawned_ || !context_.mapManager) {
		return;
	}

	const LevelData& level = context_.mapManager->GetLevelData();
	const int gridX = static_cast<int>(std::round(playerWorldPos.x / level.tileSize));
	const int gridZ = static_cast<int>(std::round(playerWorldPos.z / level.tileSize));

	if (gridX == stairsX_ && gridZ == stairsZ_) {
		requestReturnToTitle_ = true;
	}
}

void Tutorial::ClearGameplayObjects() {
	if (context_.enemyManager) {
		context_.enemyManager->Clear();
	}

	if (context_.cardPickupManager) {
		context_.cardPickupManager->Initialize(context_.camera);
	}
}

void Tutorial::BuildTutorialMap() {
	LevelData& level = const_cast<LevelData&>(context_.mapManager->GetLevelData());

	for (int z = 0; z < level.height; ++z) {
		for (int x = 0; x < level.width; ++x) {
			level.tiles[z][x] = 1;
		}
	}

	CarveRect(room1_, 0);
	CarveRect(room2_, 0);
	CarveRect(room3_, 0);

	context_.mapManager->SetCurrentFloor(1);
	context_.mapManager->SetStairsTile({ -1, -1 });
	context_.mapManager->RebuildMapObjects();

	if (context_.minimap) {
		context_.minimap->SetLevelData(&context_.mapManager->GetLevelData());
	}
}

void Tutorial::CarveRect(const Rect& rect, int tile) {
	for (int z = rect.top; z <= rect.bottom; ++z) {
		for (int x = rect.left; x <= rect.right; ++x) {
			SetTile(x, z, tile);
		}
	}
}

void Tutorial::OpenCorridor(const Rect& rect) {
	CarveRect(rect, 0);
	context_.mapManager->RebuildMapObjects();
}

void Tutorial::SetTile(int x, int z, int tile) {
	LevelData& level = const_cast<LevelData&>(context_.mapManager->GetLevelData());

	if (x < 0 || x >= level.width || z < 0 || z >= level.height) {
		return;
	}

	level.tiles[z][x] = tile;
}

Vector3 Tutorial::GetTileWorldPosition(int tileX, int tileZ, float yOffset) const {
	const LevelData& level = context_.mapManager->GetLevelData();

	return {
		tileX * level.tileSize,
		context_.mapManager->GetFloorSurfaceY(yOffset),
		tileZ * level.tileSize
	};
}


void Tutorial::PlacePlayerAt(int tileX, int tileZ) {
	if (!context_.playerManager) {
		return;
	}

	const Vector3 playerPos = GetTileWorldPosition(tileX, tileZ, 1.5f);
	context_.playerManager->SetPosition(playerPos);

	if (context_.camera) {
		context_.camera->SetTranslation({
			playerPos.x,
			playerPos.y + 15.0f,
			playerPos.z - 15.0f
			});
		context_.camera->SetRotation({ 0.9f, 0.0f, 0.0f });
		context_.camera->Update();
	}
}


void Tutorial::SpawnTutorialCard() {
	context_.cardPickupManager->Initialize(context_.camera);

	const int cardX = room1_.right - 2;
	const int cardZ = room1_.CenterZ();

	context_.cardPickupManager->AddPickup(
		GetTileWorldPosition(cardX, cardZ, 0.01f),
		CardDatabase::GetCardData(2)
	);

	pickupSpawned_ = true;
}


void Tutorial::SpawnTutorialEnemy() {
	if (!context_.enemyManager) {
		return;
	}

	context_.enemyManager->Clear();

	const int enemyX = room2_.CenterX();
	const int enemyZ = room2_.CenterZ();

	context_.enemyManager->SpawnEnemyAt(
		GetTileWorldPosition(enemyX, enemyZ, 1.0f),
		context_.camera
	);

	enemySpawned_ = true;
}


void Tutorial::SpawnTutorialStairs() {
	if (stairsSpawned_) {
		return;
	}

	stairsX_ = room3_.CenterX();
	stairsZ_ = room3_.CenterZ();

	SetTile(stairsX_, stairsZ_, 3);
	context_.mapManager->SetStairsTile({ stairsX_, stairsZ_ });
	context_.mapManager->RebuildMapObjects();

	stairsSpawned_ = true;
}

bool Tutorial::AreAllPickupsCollected() const {
	if (!context_.cardPickupManager) {
		return false;
	}

	for (const auto& pickup : context_.cardPickupManager->GetPickups()) {
		if (pickup.isActive) {
			return false;
		}
	}
	return true;
}

bool Tutorial::AreAllEnemiesDefeated() const {
	if (!context_.enemyManager) {
		return false;
	}

	const auto& enemies = context_.enemyManager->GetEnemies();
	if (enemies.empty()) {
		return false;
	}

	for (const auto& enemy : enemies) {
		if (enemy && !enemy->IsDead()) {
			return false;
		}
	}
	return true;
}

void Tutorial::UpdateTexts() const {
	TextManager* text = TextManager::GetInstance();
	text->SetPosition("TutorialTitle", 40.0f, 40.0f);
	text->SetPosition("TutorialBody", 40.0f, 90.0f);
	text->SetCentered("TutorialTitle", false);
	text->SetCentered("TutorialBody", false);

	switch (step_) {
	case Step::PickCard:
		text->SetText("TutorialTitle", "TUTORIAL 1 / 3");
		text->SetText("TutorialBody", "1つ目の部屋でカードを拾ってください。\n拾うまで次の部屋には進めません。");
		break;

	case Step::DefeatEnemy:
		text->SetText("TutorialTitle", "TUTORIAL 2 / 3");
		text->SetText("TutorialBody", "2つ目の部屋の敵を倒してください。\n倒すまで最後の部屋には進めません。");
		break;

	case Step::ReachStairs:
		text->SetText("TutorialTitle", "TUTORIAL 3 / 3");
		text->SetText("TutorialBody", "最後の部屋に階段が出ました。\n階段に乗るとタイトルへ戻ります。");
		break;
	}
}

void Tutorial::ClearTexts() const {
	TextManager::GetInstance()->SetText("TutorialTitle", "");
	TextManager::GetInstance()->SetText("TutorialBody", "");
}
