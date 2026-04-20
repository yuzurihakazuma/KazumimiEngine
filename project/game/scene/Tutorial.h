// Tutorial.h
#pragma once

#include <utility>
#include "engine/math/VectorMath.h"

class MapManager;
class PlayerManager;
class EnemyManager;
class Enemy;
class CardPickupManager;
class Camera;
class Minimap;
class Input;

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
	void Update(Input* input);
	void Finalize();
	void SetTextSuppressed(bool suppressed);

	bool IsActive() const { return isActive_; }
	bool IsNormalFloorTransitionBlocked() const { return isActive_; }
	bool IsGameplayPausedByTutorial() const { return isPauseStep_; }
	bool ConsumeReturnToTitleRequest();
	bool ConsumeAdvanceInputRequest();

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
		MoveIntro,
		PickCard,
		StatusIntro,
		CombatIntro,
		DefeatEnemy,
		CardSwapIntro,
		CardSwapPractice,
		EnemyCardWatch,
		EnemyCardIntro,
		EnemyCardBattle,
		LevelUpIntro,
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
	void SpawnCardSwapTutorialCards();
	void SpawnEnemyCardTutorial();
	void SpawnTutorialStairs();

	bool IsInsideRect(int x, int z, const Rect& rect) const;
	bool AreAllPickupsCollected() const;
	bool AreAllEnemiesDefeated() const;
	bool HasEnemyPickedTutorialCard() const;

	void UpdateTexts() const;
	void ClearTexts() const;

private:
	Context context_{};
	bool isActive_ = false;
	bool requestReturnToTitle_ = false;

	bool pickupSpawned_ = false;
	bool enemySpawned_ = false;
	bool swapPickupSpawned_ = false;
	bool enemyCardTutorialSpawned_ = false;
	bool stairsSpawned_ = false;
	bool isPauseStep_ = false;
	bool waitingForSpace_ = false;
	bool isTextSuppressed_ = false;
	bool consumedAdvanceInput_ = false;
	int tutorialAdvanceCooldown_ = 0;

	Step step_ = Step::MoveIntro;
	int stairsX_ = -1;
	int stairsZ_ = -1;

	const Rect room0_{ 6, 6, 15, 15 };
	const Rect room1_{ 6, 20, 15, 29 };
	const Rect room2_{ 20, 20, 29, 29 };
	const Rect room3_{ 34, 20, 43, 29 };
	const Rect room4_{ 34, 34, 43, 43 };
	const Rect room5_{ 20, 34, 29, 43 };
	const Rect corridor0_{ 9, 16, 12, 19 };
	const Rect corridor1_{ 16, 23, 19, 26 };
	const Rect corridor2_{ 30, 23, 33, 26 };
	const Rect corridor3_{ 37, 30, 40, 33 };
	const Rect corridor4_{ 30, 37, 33, 40 };
	static constexpr int kAdvanceCooldownFrames_ = 120;
};
