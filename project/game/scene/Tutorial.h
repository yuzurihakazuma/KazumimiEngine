// Tutorial.h
#pragma once

#include <utility>
#include "engine/math/VectorMath.h"

class MapManager;
class PlayerManager;
class EnemyManager;
class CardPickupManager;
class Camera;
class Minimap;

class Tutorial {
public:
	struct Context {
		MapManager* mapManager = nullptr;
		PlayerManager* playerManager = nullptr;
		EnemyManager* enemyManager = nullptr;
		CardPickupManager* cardPickupManager = nullptr;
		Camera* camera = nullptr;
		Minimap* minimap = nullptr;
	};

	void Initialize(const Context& context);
	void Start();
	void Update();
	void Finalize();

	bool IsActive() const { return isActive_; }
	bool IsNormalFloorTransitionBlocked() const { return isActive_; }
	bool ConsumeReturnToTitleRequest();

	// プレイヤー位置からタイル判定して、最後の階段に乗ったかを調べる
	void CheckPlayerGoal(const Vector3& playerWorldPos);

private:
	struct Rect {
		int left;
		int top;
		int right;
		int bottom;

		int CenterX() const { return (left + right) / 2; }
		int CenterZ() const { return (top + bottom) / 2; }
	};

	enum class Step {
		PickCard,
		DefeatEnemy,
		ReachStairs
	};

private:
	void ClearGameplayObjects();
	void BuildTutorialMap();
	void CarveRect(const Rect& rect, int tile);
	void OpenCorridor(const Rect& rect);
	void SetTile(int x, int z, int tile);

	Vector3 GetTileWorldPosition(int tileX, int tileZ, float yOffset) const;
	void PlacePlayerAt(int tileX, int tileZ);

	void SpawnTutorialCard();
	void SpawnTutorialEnemy();
	void SpawnTutorialStairs();

	bool AreAllPickupsCollected() const;
	bool AreAllEnemiesDefeated() const;

	void UpdateTexts() const;
	void ClearTexts() const;

private:
	Context context_{};
	bool isActive_ = false;
	bool requestReturnToTitle_ = false;

	bool pickupSpawned_ = false;
	bool enemySpawned_ = false;
	bool stairsSpawned_ = false;

	Step step_ = Step::PickCard;
	int stairsX_ = -1;
	int stairsZ_ = -1;

	const Rect room1_{ 4, 20, 13, 29 };
	const Rect room2_{ 20, 20, 29, 29 };
	const Rect room3_{ 36, 20, 45, 29 };
	const Rect corridor1_{ 14, 23, 19, 26 };
	const Rect corridor2_{ 30, 23, 35, 26 };
};
