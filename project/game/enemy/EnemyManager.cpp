#include "game/enemy/EnemyManager.h"
#include "game/enemy/Boss.h"
#include "game/player/Player.h"
#include "game/spawn/SpawnManager.h"
#include "game/card/CardUseSystem.h" 
#include "game/card/CardPickupManager.h"
#include "game/card/CardDatabase.h"
#include "engine/utils/Level/LevelEditor.h" 
#include "engine/math/VectorMath.h"
#include "game/map/Minimap.h"
#include "game/map/MapManager.h"

#include <cmath>
#include <algorithm>

using namespace VectorMath;


void EnemyManager::Initialize() {
	// 敵関連のデータをすべて初期化
	enemies_.clear();
	enemyObjs_.clear();
	enemyDeadHandled_.clear();
	enemyCardSystems_.clear();
}

void EnemyManager::Update(Player *player, CardPickupManager *cardPickupManager, MapManager* mapManager,Boss *boss) {
	if (!player || !cardPickupManager || !mapManager) return;

	Vector3 targetPos = player->GetPosition();
	const LevelData &level = mapManager->GetLevelData();

	// 敵全員分ループ
	for (size_t i = 0; i < enemies_.size(); ++i) {
		auto& enemy = enemies_[i];
		if (!enemy) {
			continue;
		}

		// 死亡時の処理を一度だけ行う
		if (enemy->IsDead()) {
			if (!enemyDeadHandled_[i]) {
				if (player) {
					player->AddExp(1);
				}
				if (enemy->HasPickupCard()) {
					cardPickupManager->AddPickup(enemy->GetPosition(), enemy->GetPickupCard());
					enemy->ClearPickupCard();
				}
				enemyDeadHandled_[i] = true;
			}
			continue;
		}

		// --- ① 移動前の座標を保存 ---
		Vector3 oldEnemyPos = enemy->GetPosition();

		// --- ② プレイヤーの位置を教える ---
		enemy->SetPlayerPosition(targetPos);

		// --- ③ 近くに落ちているカードを探す（引数のcardPickupManagerを使う） ---
		bool foundCard = false;
		Vector3 nearestCardPos{};
		float nearestCardDist = 99999.0f;

		for (auto &pickup : cardPickupManager->GetPickups()) {
			if (!pickup.isActive) continue;
			Vector3 diff = { pickup.position.x - oldEnemyPos.x, 0.0f, pickup.position.z - oldEnemyPos.z };
			float dist = Length(diff);
			if (dist < 6.0f && dist < nearestCardDist) {
				nearestCardDist = dist;
				nearestCardPos = pickup.position;
				foundCard = true;
			}
		}

		// --- ④ AI更新 ---
		enemy->SetCardTarget(foundCard, nearestCardPos);
		enemy->Update();

		// --- ⑤ 壁との衝突判定（引数のlevelを使う） ---
		Vector3 enemyPos = enemy->GetPosition();
		AABB enemyAABB;
		enemyAABB.min = { enemyPos.x - 0.5f, enemyPos.y - 0.5f, enemyPos.z - 0.5f };
		enemyAABB.max = { enemyPos.x + 0.5f, enemyPos.y + 0.5f, enemyPos.z + 0.5f };

		int enemyGridX = static_cast<int>(std::round(enemyPos.x / level.tileSize));
		int enemyGridZ = static_cast<int>(std::round(enemyPos.z / level.tileSize));
		int eStartX = (std::max)(0, enemyGridX - 1);
		int eEndX = (std::min)(level.width - 1, enemyGridX + 1);
		int eStartZ = (std::max)(0, enemyGridZ - 1);
		int eEndZ = (std::min)(level.height - 1, enemyGridZ + 1);

		bool isEnemyHit = false;
		for (int z = eStartZ; z <= eEndZ && !isEnemyHit; z++) {
			for (int x = eStartX; x <= eEndX; x++) {
				if (level.tiles[z][x] != 1) {
					continue;
				}

				float worldX = x * level.tileSize;
				float worldZ = z * level.tileSize;
				AABB blockAABB;
				blockAABB.min = { worldX - 1.0f, level.baseY, worldZ - 1.0f };
				blockAABB.max = { worldX + 1.0f, level.baseY + 2.0f, worldZ + 1.0f };

				if (Collision::IsCollision(enemyAABB, blockAABB)) {
					enemy->SetPosition(oldEnemyPos);
					isEnemyHit = true;
					break;
				}
			}
		}

		// --- ⑥ 見た目（3Dモデル）に座標を反映 ---
		if (enemyObjs_[i]) {
			enemyObjs_[i]->SetTranslation(enemy->GetPosition());
			enemyObjs_[i]->SetRotation(enemy->GetRotation());
			enemyObjs_[i]->Update();
		}
		// 敵が「カードを使いたい！」と合図を出した時の処理
		if (enemy->GetCardUseRequest()) {
			if (enemyCardSystems_[i]) {
				enemyCardSystems_[i]->UseCard(
					enemy->GetCurrentUseCard(),
					enemy->GetPosition(),
					enemy->GetRotation().y,
					false // プレイヤーではないので false
				);
			}
			// 合図を出したらリセットする
			enemy->ClearCardUseRequest();
		}

		// 敵の魔法を毎フレーム動かす（飛んでいる弾などの処理）
		if (enemyCardSystems_[i]) {
			// CardUseSystem が EnemyManager を要求するようになったので "this"（自分自身）を渡す！
			enemyCardSystems_[i]->Update(
				player,
				this,      // EnemyManager 自身を渡す
				nullptr,   // ボスのポインタ（雑魚同士やボスに当たらないなら nullptr でOK）
				player->GetPosition(),
				enemy->GetPosition(),
				{ 0.0f, 0.0f, 0.0f }, // bossPos
				mapManager->GetLevelData()
			);
		}

		// 敵がカードを拾う処理（復活！）
		if (!enemy->HasPickupCard()) {
			for (auto &pickup : cardPickupManager->GetPickups()) {
				if (!pickup.isActive) continue;

				Vector3 ePos = enemy->GetPosition();
				Vector3 diff = {
					ePos.x - pickup.position.x,
					ePos.y - pickup.position.y,
					ePos.z - pickup.position.z
				};

				// 距離が近ければ拾う
				if (Length(diff) < 2.0f) {
					Card pickedCard = pickup.card;
					// 敵が使えないカードなら、使えるカードにすり替える
					if (!pickedCard.canEnemyUse) {
						pickedCard = CardDatabase::GetRandomEnemyUsableCard();
					}
					enemy->SetPickupCard(pickedCard);
					pickup.isActive = false; // マップ上からカードを消す
					break; // 1度に拾うのは1枚だけ
				}
			}
		}
	}
	// =========================================================
	// ボスとの押し出し判定
	// =========================================================
	if (boss && !boss->IsDead()) {
		Vector3 bossPos = boss->GetPosition();
		float bossRadius = 3.0f; // ボスの大きさ（適宜調整してください）

		for (auto &enemy : enemies_) {
			if (enemy && !enemy->IsDead()) {
				Vector3 enemyPos = enemy->GetPosition();
				float enemyRadius = 1.0f;

				Vector3 diff = { enemyPos.x - bossPos.x, 0.0f, enemyPos.z - bossPos.z };
				float dist = Length(diff);
				float pushRange = bossRadius + enemyRadius;

				if (dist > 0.01f && dist < pushRange) {
					Vector3 pushDir = Normalize(diff);
					float pushAmount = pushRange - dist;
					enemyPos.x += pushDir.x * pushAmount;
					enemyPos.z += pushDir.z * pushAmount;
					enemy->SetPosition(enemyPos);
				}
			}
		}
	}
}


void EnemyManager::Draw(Camera *camera, Minimap *minimap) {
	std::vector<Vector3> enemyPositions;

	// 1. 敵の見た目（3Dモデル）の描画 ＆ ミニマップ用の座標集め
	for (size_t i = 0; i < enemies_.size(); ++i) {
		if (enemies_[i] && !enemies_[i]->IsDead()) {
			// 生きている敵だけモデルを描画する
			if (enemyObjs_[i]) {
				enemyObjs_[i]->Draw(); // ※引数が必要な場合は camera を渡す
			}
			// ミニマップ用の座標を保存
			enemyPositions.push_back(enemies_[i]->GetPosition());
		}
	}

	// 2. 魔法（カード効果）の描画
	for (auto &cardSystem : enemyCardSystems_) {
		if (cardSystem) {
			cardSystem->Draw();
		}
	}

	// 3. ミニマップへ座標を渡す
	if (minimap) {
		minimap->SetEnemyPositions(enemyPositions);
	}
}

void EnemyManager::SpawnBossMinions(int spawnCount, const Vector3 &summonCenter, Camera *camera) {
	// 現在「生きている雑魚敵」の数を数える
	int aliveCount = 0;
	for (const auto &enemy : enemies_) {
		if (enemy && !enemy->IsDead()) {
			aliveCount++;
		}
	}

	// 上限の設定
	int maxMinions = 5; // 出したい上限の数
	if (aliveCount >= maxMinions) {
		return; // すでに上限まで生きているなら、召喚をキャンセル！
	}

	// 今回実際に召喚する数を計算（要求数か、上限までの空き枠の少ない方）
	int actualSpawnCount = std::min(spawnCount, maxMinions - aliveCount);

	// 召喚数が0なら何もしない
	if (actualSpawnCount <= 0) {
		return;
	}

	// 4. 実際に召喚する処理
	float angleStep = 3.14159f * 2.0f / actualSpawnCount;
	for (int i = 0; i < actualSpawnCount; ++i) {
		float angle = angleStep * i;
		float radius = 4.0f;
		Vector3 spawnPos = {
			summonCenter.x + std::sin(angle) * radius,
			summonCenter.y,
			summonCenter.z + std::cos(angle) * radius
		};

		// 敵の本体を生成
		auto newEnemy = std::make_unique<Enemy>();
		newEnemy->Initialize();
		newEnemy->SetPosition(spawnPos);

		// 敵の見た目（3Dモデル）を生成
		auto enemyObj = std::unique_ptr<Obj3d>(Obj3d::Create("enemy"));
		if (enemyObj) {
			enemyObj->SetCamera(camera);
			enemyObj->SetTranslation(spawnPos);
			enemyObj->SetScale({ 1.0f, 1.0f, 1.0f });
			enemyObj->Update();
		}

		// 敵の魔法システムを生成
		auto enemyCardSystem = std::make_unique<CardUseSystem>();
		enemyCardSystem->Initialize(camera);

		// リストに追加
		enemies_.push_back(std::move(newEnemy));
		enemyObjs_.push_back(std::move(enemyObj));
		enemyCardSystems_.push_back(std::move(enemyCardSystem));
		enemyDeadHandled_.push_back(false);
	}
}

void EnemyManager::SpawnEnemiesRandom(int enemyCount, int margin, SpawnManager *spawnManager, MapManager* mapManager, const Vector3 &playerPos, Camera *camera) {
	// ポインタが有効かチェック
	if (!spawnManager || !mapManager || !spawnManager->HasLevelData()) {
		return;
	}

	const LevelData &level = mapManager->GetLevelData();

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

		// マップの範囲内かチェック
		if (x >= 0 && x < level.width && z >= 0 && z < level.height) {
			if (level.tiles[z][x] != 0) {
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

	// 生きている敵の数を数える
	int aliveCount = 0;
	for (const auto& enemy : enemies_) {
		if (enemy && !enemy->IsDead()) {
			aliveCount++;
		}
	}

	int availableSpace = kMaxEnemies - aliveCount;

	if (availableSpace <= 0) {
		return;
	}

	int actualSpawnCount = (std::min)(enemyCount, static_cast<int>(filtered.size()));
	actualSpawnCount = (std::min)(actualSpawnCount, availableSpace);

	for (int i = 0; i < actualSpawnCount; ++i) {
		int tileX = filtered[i].first;
		int tileZ = filtered[i].second;

		
		// spawnManager-> に変更
		Vector3 worldPos = spawnManager->TileToWorldPosition(tileX, tileZ,0.0f);

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

void EnemyManager::Clear() {
	enemies_.clear();
	enemyObjs_.clear();
	enemyCardSystems_.clear(); 
	enemyDeadHandled_.clear();
}

void EnemyManager::CheckCollisions(Player *player) {
	if (!player || player->IsDead()) {
		return;
	}

	// 1. プレイヤーの当たり判定
	Vector3 pPos = player->GetPosition();
	AABB playerAABB;
	playerAABB.min = { pPos.x - 0.5f, pPos.y - 0.5f, pPos.z - 0.5f };
	playerAABB.max = { pPos.x + 0.5f, pPos.y + 0.5f, pPos.z + 0.5f };

	// 2. 敵との接触判定
	for (auto &enemy : enemies_) {
		if (!enemy || enemy->IsDead()) continue;

		Vector3 ePos = enemy->GetPosition();
		AABB enemyAABB;
		enemyAABB.min = { ePos.x - 0.5f, ePos.y - 0.5f, ePos.z - 0.5f };
		enemyAABB.max = { ePos.x + 0.5f, ePos.y + 0.5f, ePos.z + 0.5f };

		// 体と体がぶつかったら
		if (Collision::IsCollision(enemyAABB, playerAABB)) {
			// 例：プレイヤーに接触ダメージ（必要であれば）
			// player->TakeDamage(1);
		}
	}
}
