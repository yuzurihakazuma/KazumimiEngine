#include "game/enemy/EnemyManager.h"
#include "game/spawn/SpawnManager.h"
#include "engine/utils/Level/LevelEditor.h" 
#include "game/card/CardUseSystem.h" 
#include <cmath>
#include <algorithm>


void EnemyManager::Initialize() {
	enemies_.clear();
}

void EnemyManager::Update(Player *player) {
	// ★変更：auto を使わず、i を使って回す形にします（頭脳と見た目をペアで扱うため）
	for (size_t i = 0; i < enemies_.size(); ++i) {

		// ① 頭脳（Enemy）の更新
		enemies_[i]->Update();

		// ② 見た目（Obj3d）を頭脳の座標に合わせて動かす＆更新する！
		if (enemyObjs_[i]) {
			enemyObjs_[i]->SetTranslation(enemies_[i]->GetPosition());
			enemyObjs_[i]->SetRotation(enemies_[i]->GetRotation());
			enemyObjs_[i]->SetScale(enemies_[i]->GetScale());

			// これが超重要です！毎フレームこれを呼ばないと画面に出ません！
			enemyObjs_[i]->Update();
		}
	}

	//// HPが0になって死んだ敵を、リストから自動的に削除する魔法のコード！
	//enemies_.erase(std::remove_if(enemies_.begin(), enemies_.end(),
	//	[](const std::unique_ptr<Enemy> &e) { return e->IsDead(); }), 
	//	enemies_.end());
}

void EnemyManager::Draw(Camera *camera) {
	// 敵の見た目（3Dモデル）を描画する！
	for (const auto &obj : enemyObjs_) {
		if (obj) {
			obj->Draw(); // ※引数にcameraが必要な場合は obj->Draw(camera); にしてください
		}
	}
}

void EnemyManager::SpawnBossMinions(int spawnCount, const Vector3 &summonCenter, Camera *camera) {
	const int kMaxMinions = 5;
	int currentEnemies = static_cast<int>(enemies_.size());
	int availableSpace = kMaxMinions - currentEnemies;

	if (availableSpace <= 0) {
		return;
	}

	int actualSpawnCount = std::min(spawnCount, availableSpace);
	float radius = 4.0f; // ボスからの距離

	for (int i = 0; i < actualSpawnCount; ++i) {
		// ボスの周りに円形に配置する計算
		float angle = (360.0f / actualSpawnCount) * i;
		float radian = angle * (3.14159f / 180.0f);
		Vector3 spawnPos = {
			summonCenter.x + std::sinf(radian) * radius,
			summonCenter.y,
			summonCenter.z + std::cosf(radian) * radius
		};

		// ① 敵の本体を生成
		auto newEnemy = std::make_unique<Enemy>();
		newEnemy->Initialize();
		newEnemy->SetPosition(spawnPos);
		enemies_.push_back(std::move(newEnemy));

		// ② 敵の3Dモデルを生成
		auto enemyObj = std::unique_ptr<Obj3d>(Obj3d::Create("enemy"));
		if (enemyObj) {
			enemyObj->SetCamera(camera);
			enemyObj->SetTranslation(spawnPos);
			enemyObj->SetScale({ 1.0f, 1.0f, 1.0f });
			enemyObj->Update();
		}
		enemyObjs_.push_back(std::move(enemyObj));

		// ③ 死亡処理フラグを追加
		enemyDeadHandled_.push_back(false);

		// ④ 魔法システムを生成 (※CardUseSystemの準備ができていればコメント解除してください)
		/* auto enemyCardSystem = std::make_unique<CardUseSystem>();
		 enemyCardSystem->Initialize(camera);
		 enemyCardSystems_.push_back(std::move(enemyCardSystem));*/
	}
}

void EnemyManager::SpawnEnemiesRandom(int enemyCount, int margin, SpawnManager *spawnManager, LevelEditor *levelEditor, const Vector3 &playerPos, Camera *camera) {
	// ポインタが有効かチェック
	if (!spawnManager || !levelEditor || !spawnManager->HasLevelData()) {
		return;
	}

	const LevelData &level = levelEditor->GetLevelData();

	// spawnManager-> に変更
	std::vector<std::pair<int, int>> candidates = spawnManager->FindEnemySpawnCandidates(margin);

	if (candidates.empty()) {
		return;
	}

	// プレイヤーの現在位置をタイル座標に変換
	int playerTileX = static_cast<int>(std::round(playerPos.x / level.tileSize));
	int playerTileZ = static_cast<int>(std::round(playerPos.z / level.tileSize));

	std::vector<std::pair<int, int>> filtered;
	for (const auto &c : candidates) {
		int x = c.first;
		int z = c.second;

		// 階段本体 + 周囲1マス禁止（※IsNearStairsTileの処理は、必要ならEnemyManagerに持ってくるか、SpawnManagerに移動させると良いです。今回は一旦コメントアウトか簡易的な判定にします）
		// if (IsNearStairsTile(x, z)) { continue; } 

		if (x >= 0 && x < level.width && z >= 0 && z < level.height) {
			if (level.tiles[z][x] == 3) {
				continue;
			}
		}

		// プレイヤーから近すぎるマスを除外
		int dx = x - playerTileX;
		int dz = z - playerTileZ;
		float distanceToPlayer = std::sqrt(static_cast<float>(dx * dx + dz * dz));

		if (distanceToPlayer < 5.0f) {
			continue;
		}

		filtered.push_back(c);
	}

	if (filtered.empty()) {
		return;
	}

	std::random_device rd;
	std::mt19937 mt(rd());
	std::shuffle(filtered.begin(), filtered.end(), mt);

	const int kMaxEnemies = 5;
	int currentEnemies = static_cast<int>(enemies_.size());
	int availableSpace = kMaxEnemies - currentEnemies;

	if (availableSpace <= 0) {
		return;
	}

	int actualSpawnCount = (std::min)(enemyCount, static_cast<int>(filtered.size()));
	actualSpawnCount = (std::min)(actualSpawnCount, availableSpace);

	for (int i = 0; i < actualSpawnCount; ++i) {
		int tileX = filtered[i].first;
		int tileZ = filtered[i].second;

		// spawnManager-> に変更
		Vector3 worldPos = spawnManager->TileToWorldPosition(tileX, tileZ, 0.0f);

		auto enemy = std::make_unique<Enemy>();
		enemy->Initialize();
		enemy->SetPosition(worldPos);
		enemy->SetScale({ 1.0f, 1.0f, 1.0f });

		auto enemyObj = std::unique_ptr<Obj3d>(Obj3d::Create("enemy"));
		if (enemyObj) {
			enemyObj->SetCamera(camera); // camera_.get() から camera に変更
			enemyObj->SetTranslation(worldPos);
			enemyObj->SetScale({ 1.0f, 1.0f, 1.0f });
			enemyObj->Update();
		}

		auto enemyCardSystem = std::make_unique<CardUseSystem>();
		enemyCardSystem->Initialize(camera); // 同上

		enemies_.push_back(std::move(enemy));
		enemyObjs_.push_back(std::move(enemyObj));
		enemyDeadHandled_.push_back(false);
		enemyCardSystems_.push_back(std::move(enemyCardSystem));
	}
}
