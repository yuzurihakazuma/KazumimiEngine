#include "GamePlayScene.h"
// --- ゲーム固有のファイル ---
#include "TitleScene.h"

// --- エンジン側のファイル ---
#include "Engine/Math/Matrix4x4.h"
#include "Engine/Utils/ImGuiManager.h"
#include "Engine/Utils/Color.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/3D/Model/ModelManager.h"
#include "Engine/Particle/ParticleManager.h"
#include "Engine/Graphics/TextureManager.h"
#include "Engine/Graphics/PipelineManager.h"
#include "Engine/Scene/SceneManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Camera/DebugCamera.h"
#include "Engine/2D/Sprite.h"
#include "Engine/3D/Obj/Obj3d.h"
#include "Engine/Base/Input.h"
#include "Engine/2D/SpriteCommon.h"
#include "Engine/3D/Obj/Obj3dCommon.h"
#include "Engine/Base/DirectXCommon.h"
#include "Engine/Base/WindowProc.h"
#include "engine/math/VectorMath.h"
#include "engine/collision/Collision.h"
#include "engine/graphics/RenderTexture.h"
#include "engine/graphics/SrvManager.h"
#include "engine/postEffect/PostEffect.h"
#include "game/player/Player.h"
#include"engine/utils/Level/LevelEditor.h"
#include "engine/utils/TextManager.h"
#include "game/enemy/Enemy.h"
#include "game/enemy/Boss.h"
#include "game/card/CardUseSystem.h"


using namespace VectorMath;
using namespace MatrixMath;
// 初期化
void GamePlayScene::Initialize() {
	DirectXCommon *dxCommon = DirectXCommon::GetInstance();
	WindowProc *windowProc = WindowProc::GetInstance();

	// コマンドリスト取得
	auto commandList = dxCommon->GetCommandList();

	// BGMロード (シングルトン)
	AudioManager::GetInstance()->LoadWave(bgmFile_);
	// モデル読み込み (シングルトン)
	ModelManager::GetInstance()->LoadModel("fence", "resources", "fence.obj");

	ModelManager::GetInstance()->LoadModel("grass", "resources", "terrain.obj");
	ModelManager::GetInstance()->LoadModel("block", "resources/block", "block.obj");

	// 球モデル作成 (シングルトン)
	ModelManager::GetInstance()->CreateSphereModel("sphere", 16);
	// パーティクルグループ作成 (シングルトン)
	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "resources/uvChecker.png");

	// テクスチャ読み込み
	textures_["uvChecker"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/uvChecker.png", commandList);
	textures_["monsterBall"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/monsterBall.png", commandList);
	textures_["fence"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/fence.png", commandList);
	textures_["circle"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/circle.png", commandList);
	textures_["noise0"] = { TextureManager::GetInstance()->LoadTextureAndCreateSRV("Resources/noise0.png", commandList) };
	textures_["noise1"] = { TextureManager::GetInstance()->LoadTextureAndCreateSRV("Resources/noise1.png", commandList) };

	// カメラ生成
	camera_ = std::make_unique<Camera>(windowProc->GetClientWidth(), windowProc->GetClientHeight(), dxCommon);
	camera_->SetTranslation({ 0.0f, 2.0f, -15.0f });

	//UI専用カメラの初期化
	uiCamera_ = std::make_unique<Camera>(1280, 720, dxCommon);

	// デバッグカメラ生成
	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize();


	playerObj_ = Obj3d::Create("sphere");
	if (playerObj_) {
		playerObj_->SetCamera(camera_.get());
		playerObj_->SetTranslation(playerPos_);
		playerObj_->SetScale(playerScale_);
	}

	player_ = std::make_unique<Player>();
	player_->Initialize();
	player_->SetPosition(playerPos_);
	player_->SetScale(playerScale_);

	boss_ = std::make_unique<Boss>();
	boss_->Initialize();
	boss_->SetScale({ 2.0f, 2.0f, 2.0f });

	bossObj_ = std::unique_ptr<Obj3d>(Obj3d::Create("sphere"));
	if (bossObj_) {
		bossObj_->SetCamera(camera_.get());
		bossObj_->SetScale(boss_->GetScale());
	}
	bossDeadHandled_ = false;

	// カード使用システム初期化
	playerCardSystem_ = std::make_unique<CardUseSystem>();
	playerCardSystem_->Initialize(camera_.get());

	bossCardSystem_ = std::make_unique<CardUseSystem>();
	bossCardSystem_->Initialize(camera_.get());

	// ファイル名を指定するだけで、読み込み・生成・配置
	// 引数: (ファイルパス, 座標)
	sprite_ = Sprite::Create(textures_["uvChecker"].srvIndex, spritePos_);
	// プレイヤーオブジェクト生成
	testObj_ = Obj3d::Create("block");
	if (testObj_) {

		testObj_->SetCamera(camera_.get());
		testObj_->SetTranslation({ 0.0f, 0.0f, 5.0f });

		// ノイズ画像と初期の閾値(0.0)をセット
		testObj_->SetNoiseTexture(textures_["noise0"].srvIndex);
		testObj_->SetDissolveThreshold(0.0f);

	}

	// デプスステンシル作成 (TextureManagerシングルトン)
	depthStencilResource_ = TextureManager::GetInstance()->CreateDepthStencilTextureResource(
		windowProc->GetClientWidth(), windowProc->GetClientHeight()
	);

	currentFloor_ = 1;
	// マップエディタ生成・初期化
	// マップエディタ生成・初期化
	levelEditor_ = std::make_unique<LevelEditor>();
	levelEditor_->SetCamera(camera_.get());
	levelEditor_->Initialize();

	// カード用の3Dモデルを読み込んでおく（※パスやファイル名はご自身の環境に合わせてください）
	ModelManager::GetInstance()->LoadModel("plane", "resources/plane", "plane.obj");
	ModelManager::GetInstance()->LoadModel("cardR", "resources/card", "CardR.obj");
	ModelManager::GetInstance()->LoadModel("cardF", "resources/card", "cardF.obj");
	ModelManager::GetInstance()->LoadModel("cardFire", "resources/card", "CardFire.obj");
	ModelManager::GetInstance()->LoadModel("cardPotion", "resources/card", "CardPotion.obj");
	ModelManager::GetInstance()->LoadModel("cardSpeedUp", "resources/card", "CardSpeedUp.obj");

	// CSVからカードデータベースを初期化
	CardDatabase::Initialize("Resources/CardData.csv");

	// 手札マネージャーの初期化
	handManager_.Initialize(uiCamera_.get(), textures_["noise0"].srvIndex);

	//最初から手札にID１を追加する
	handManager_.AddCard(CardDatabase::GetCardData(1));

	// これだけでOK
	RegenerateDungeonAndRespawnPlayer(8);

	// 初期ロード時のマップ変更通知を消す
	if (levelEditor_) {
		levelEditor_->ConsumeMapChanged();
	}

	TextManager::GetInstance()->Initialize();
}

void GamePlayScene::Update() {

	// デバッグカメラ更新
	if (debugCamera_) {
		debugCamera_->Update(camera_.get());
	}

	Input *input = Input::GetInstance();

	// マップ切り替えがあったら戦闘ごとリセット
	if (levelEditor_ && levelEditor_->ConsumeMapChanged()) {
		ResetBattleDebug();
		return;
	}

	if (isCardSwapMode_) {
		UpdateCardSwapMode(input);
		return;
	}

	// デバッグ用リセット
	if (input->Triggerkey(DIK_R)) {
		ResetBattleDebug();
	}

	// BGM再生
	if (input->Triggerkey(DIK_SPACE)) {
		AudioManager::GetInstance()->PlayWave(bgmFile_);
	}

	// タイトルシーンへ移動
	if (input->Triggerkey(DIK_T)) {
		SceneManager::GetInstance()->ChangeScene(std::make_unique<TitleScene>());
	}

	// パーティクル発生
	if (input->Triggerkey(DIK_P)) {
		ParticleManager::GetInstance()->Emit("Circle", { 0.0f, 0.0f, 0.0f }, 10);
	}

	// パーティクル更新
	ParticleManager::GetInstance()->Update(camera_.get());

	// ==========================================
	// プレイヤーの更新処理
	// ==========================================

	if (player_) {
		if (debugCamera_ && debugCamera_->IsActive()) {
			player_->SetInputEnable(false);
		} else {
			player_->SetInputEnable(true);
		}

		Vector3 oldPos = player_->GetPosition();

		player_->Update();
		playerPos_ = player_->GetPosition();

		AABB playerAABB;
		playerAABB.min = { playerPos_.x - 0.5f, playerPos_.y - 0.5f, playerPos_.z - 0.5f };
		playerAABB.max = { playerPos_.x + 0.5f, playerPos_.y + 0.5f, playerPos_.z + 0.5f };

		const LevelData &level = levelEditor_->GetLevelData();

		int centerGridX = static_cast<int>(std::round(playerPos_.x / level.tileSize));
		int centerGridZ = static_cast<int>(std::round(playerPos_.z / level.tileSize));

		int startX = std::max(0, centerGridX - 1);
		int endX = std::min(level.width - 1, centerGridX + 1);
		int startZ = std::max(0, centerGridZ - 1);
		int endZ = std::min(level.height - 1, centerGridZ + 1);

		bool isPlayerHit = false;

		for (int z = startZ; z <= endZ && !isPlayerHit; z++) {
			for (int x = startX; x <= endX; x++) {

				if (level.tiles[z][x] != 1) continue;

				float worldX = x * level.tileSize;
				float worldZ = z * level.tileSize;

				AABB blockAABB;
				blockAABB.min = { worldX - 1.0f, level.baseY, worldZ - 1.0f };
				blockAABB.max = { worldX + 1.0f, level.baseY + 2.0f, worldZ + 1.0f };

				if (Collision::IsCollision(playerAABB, blockAABB)) {
					player_->SetPosition(oldPos);
					playerPos_ = oldPos;
					isPlayerHit = true;
					break;
				}
			}
		}

		playerScale_ = player_->GetScale();
	}

	if (playerObj_) {
		playerObj_->SetTranslation(playerPos_);
		playerObj_->SetRotation(player_->GetRotation());
		playerObj_->SetScale(playerScale_);
		playerObj_->Update();
	}

	// ==========================================
	// 雑魚敵の更新処理
	// ==========================================

	// ターゲットを決める！
	Vector3 targetPos = playerPos_; // 基本はプレイヤーの位置を狙う
	if (playerCardSystem_ && playerCardSystem_->IsDecoyActive()) {
		targetPos = playerCardSystem_->GetDecoyPosition(); // 身代わりがいたら身代わりを狙う！
	}

	for (size_t i = 0; i < enemies_.size(); ++i) {
		auto &enemy = enemies_[i];
		if (!enemy || enemy->IsDead()) {
			continue;
		}

		// 移動前の座標を保存
		Vector3 oldEnemyPos = enemy->GetPosition();

		// プレイヤーの位置を敵に教える（追従AIなどのため）
		enemy->SetPlayerPosition(targetPos);

		// --- 近くに落ちているカードを探す処理 ---
		bool foundCard = false;
		Vector3 nearestCardPos{};
		float nearestCardDist = 99999.0f;

		for (auto &pickup : cardPickupManager_.GetPickups()) {
			if (!pickup.isActive) {
				continue;
			}

			Vector3 diff = {
				pickup.position.x - oldEnemyPos.x,
				0.0f,
				pickup.position.z - oldEnemyPos.z
			};

			float dist = Length(diff);

			// 一定距離(6.0f)以内で一番近いカードを見つける
			if (dist < 6.0f && dist < nearestCardDist) {
				nearestCardDist = dist;
				nearestCardPos = pickup.position;
				foundCard = true;
			}
		}

		// 敵のAIにカードの目標位置をセットし、更新する
		enemy->SetCardTarget(foundCard, nearestCardPos);
		enemy->Update();

		Vector3 enemyPos = enemy->GetPosition();

		// --- 敵と地形(マップブロック)の衝突判定 ---
		AABB enemyAABB;
		enemyAABB.min = { enemyPos.x - 0.5f, enemyPos.y - 0.5f, enemyPos.z - 0.5f };
		enemyAABB.max = { enemyPos.x + 0.5f, enemyPos.y + 0.5f, enemyPos.z + 0.5f };

		const LevelData &level = levelEditor_->GetLevelData();

		int enemyGridX = static_cast<int>(std::round(enemyPos.x / level.tileSize));
		int enemyGridZ = static_cast<int>(std::round(enemyPos.z / level.tileSize));

		int eStartX = std::max(0, enemyGridX - 1);
		int eEndX = std::min(level.width - 1, enemyGridX + 1);
		int eStartZ = std::max(0, enemyGridZ - 1);
		int eEndZ = std::min(level.height - 1, enemyGridZ + 1);

		bool isEnemyHit = false;

		for (int z = eStartZ; z <= eEndZ && !isEnemyHit; z++) {
			for (int x = eStartX; x <= eEndX; x++) {
				if (level.tiles[z][x] != 1) {
					continue;
				}

				float worldX = x * level.tileSize;
				float worldZ = z * level.tileSize;

				AABB blockAABB;
				blockAABB.min = { worldX - 1.0f, level.baseY,        worldZ - 1.0f };
				blockAABB.max = { worldX + 1.0f, level.baseY + 2.0f, worldZ + 1.0f };

				if (Collision::IsCollision(enemyAABB, blockAABB)) {
					enemy->SetPosition(oldEnemyPos);
					isEnemyHit = true;
					break;
				}
			}
		}
	}

	for (size_t i = 0; i < enemies_.size(); ++i) {
		auto &enemy = enemies_[i];
		auto &enemyObj = enemyObjs_[i];

		if (!enemy || !enemyObj || enemy->IsDead()) {
			continue;
		}

		enemyObj->SetTranslation(enemy->GetPosition());
		enemyObj->SetRotation(enemy->GetRotation());
		enemyObj->SetScale(enemy->GetScale());
		enemyObj->Update();
	}

	// ==========================================
	// ボスの更新処理
	// ==========================================
	if (boss_ && !boss_->IsDead() && levelEditor_ && levelEditor_->IsBossMap()) {
		Vector3 oldBossPos = boss_->GetPosition();

		// プレイヤーの位置をボスに教えてAIを更新
		boss_->SetPlayerPosition(targetPos);
		boss_->Update();

		Vector3 bossPos = boss_->GetPosition();

		// --- ボスと地形(マップブロック)の衝突判定 ---
		AABB bossAABB;
		bossAABB.min = { bossPos.x - 1.0f, bossPos.y - 1.0f, bossPos.z - 1.0f };
		bossAABB.max = { bossPos.x + 1.0f, bossPos.y + 1.0f, bossPos.z + 1.0f };

		const LevelData &level = levelEditor_->GetLevelData();

		int bossGridX = static_cast<int>(std::round(bossPos.x / level.tileSize));
		int bossGridZ = static_cast<int>(std::round(bossPos.z / level.tileSize));

		int bStartX = std::max(0, bossGridX - 1);
		int bEndX = std::min(level.width - 1, bossGridX + 1);
		int bStartZ = std::max(0, bossGridZ - 1);
		int bEndZ = std::min(level.height - 1, bossGridZ + 1);

		bool isBossHit = false;

		for (int z = bStartZ; z <= bEndZ && !isBossHit; z++) {
			for (int x = bStartX; x <= bEndX; x++) {

				if (level.tiles[z][x] != 1) {
					continue;
				}

				float worldX = x * level.tileSize;
				float worldZ = z * level.tileSize;

				AABB blockAABB;
				blockAABB.min = { worldX - 1.0f, level.baseY,        worldZ - 1.0f };
				blockAABB.max = { worldX + 1.0f, level.baseY + 2.0f, worldZ + 1.0f };

				if (Collision::IsCollision(bossAABB, blockAABB)) {
					boss_->SetPosition(oldBossPos);
					isBossHit = true;
					break;
				}
			}
		}

		if (bossObj_) {
			bossObj_->SetTranslation(boss_->GetPosition());
			bossObj_->SetRotation(boss_->GetRotation());
			bossObj_->SetScale(boss_->GetScale());
			bossObj_->Update();
		}
	}

	// =========================
    // 敵 → プレイヤー
    // =========================
	for (size_t i = 0; i < enemies_.size(); ++i) {
		auto &enemy = enemies_[i];
		if (!enemy || !player_ || enemy->IsDead() || player_->IsDead()) {
			continue;
		}

		Vector3 enemyPos = enemy->GetPosition();

		// 敵の近接攻撃要求がある場合
		if (enemy->GetAttackRequest()) {
			Vector3 diff = {
				playerPos_.x - enemyPos.x,
				0.0f,
				playerPos_.z - enemyPos.z
			};

			// 攻撃範囲内ならプレイヤーにダメージを与える
			if (Length(diff) <= 2.0f) {
				player_->TakeDamage(1, enemyPos);

				// プレイヤー用の詠唱だけ止める
				if (playerCardSystem_) {
					playerCardSystem_->CancelCasting();
				}
			}

			enemy->ClearAttackRequest();
		}

		// 敵のカード使用要求がある場合
		if (enemy->GetCardUseRequest()) {
			if (enemy->HasCard()) {
				// この敵専用のカードシステムを使う
				if (i < enemyCardSystems_.size() && enemyCardSystems_[i]) {
					enemyCardSystems_[i]->UseCard(
						enemy->GetHeldCard(),
						enemyPos,
						enemy->GetRotation().y,
						false
					);
				}

				enemy->ClearHeldCard();
			}

			enemy->ClearCardUseRequest();
		}
	}
	// =========================
	// ボス → プレイヤー
	// =========================
	if (boss_ && player_ && !boss_->IsDead() && !player_->IsDead() && levelEditor_ && levelEditor_->IsBossMap()) {
		Vector3 bossPos = boss_->GetPosition();

		// ボスの近接攻撃要求がある場合
		if (boss_->GetAttackRequest()) {
			Vector3 diff = {
				playerPos_.x - bossPos.x,
				0.0f,
				playerPos_.z - bossPos.z
			};

			// 攻撃範囲内ならプレイヤーにダメージを与える
			if (Length(diff) <= 3.0f) {
				player_->TakeDamage(2, bossPos);

				// プレイヤー用の詠唱だけ止める
				if (playerCardSystem_) {
					playerCardSystem_->CancelCasting();
				}
			}

			boss_->ClearAttackRequest();
		}

		// ボスのカード使用要求がある場合
		if (boss_->GetCardUseRequest()) {
			if (bossCardSystem_) {
				bossCardSystem_->UseCard(
					boss_->GetSelectedCard(),
					bossPos,
					boss_->GetRotation().y,
					false
				);
			}

			boss_->ClearCardUseRequest();
		}
	}
	// ==========================================
	// 死亡時の処理 (経験値・ドロップ)
	// ==========================================
	// 雑魚敵の死亡時
	for (size_t i = 0; i < enemies_.size(); ++i) {
		auto &enemy = enemies_[i];
		if (!enemy) {
			continue;
		}

		// 死んだ瞬間だけ処理するフラグ
		if (enemy->IsDead() && !enemyDeadHandled_[i]) {
			if (player_) {
				player_->AddExp(1);
			}

			// 敵がカードを持っていたらその場に落とす
			if (enemy->HasCard()) {
				cardPickupManager_.AddPickup(enemy->GetPosition(), enemy->GetHeldCard());
				enemy->ClearHeldCard();
			}

			enemyDeadHandled_[i] = true;
		}
	}

	// ボスの死亡時
	if (boss_ && boss_->IsDead() && !bossDeadHandled_) {
		if (player_) {
			player_->AddExp(5);
		}

		// ボスがカードを持っていればランダムでドロップ
		if (boss_->HasAnyCard()) {
			Card dropCard = boss_->GetRandomDropCard();
			if (dropCard.id != -1) {
				cardPickupManager_.AddPickup(boss_->GetPosition(), dropCard);
			}
		}

		bossDeadHandled_ = true;
	}

	// ==========================================
	// ドロップアイテム(カード)の取得判定
	// ==========================================

	cardPickupManager_.Update();

	for (auto &pickup : cardPickupManager_.GetPickups()) {
		if (!pickup.isActive) {
			continue;
		}

		// プレイヤーとの距離計算
		Vector3 playerDiff = {
			playerPos_.x - pickup.position.x,
			playerPos_.y - pickup.position.y,
			playerPos_.z - pickup.position.z
		};

		float playerDist = Length(playerDiff);

		// プレイヤーが拾う処理
		if (player_ && !player_->IsDead() && playerDist < 2.0f) {
			bool success = handManager_.AddCard(pickup.card);
			if (success) {
				pickup.isActive = false;
				continue;
			} else {
				// 手札が一杯ならカード交換モードへ移行
				isCardSwapMode_ = true;
				pendingCard_ = pickup.card;
				pickup.isActive = false;
				continue;
			}
		}

		// 敵が拾う処理
		for (size_t i = 0; i < enemies_.size(); ++i) {
			auto &enemy = enemies_[i];

			// 既にカードを持っている敵や死んでいる敵は拾えない
			if (!enemy || enemy->IsDead() || enemy->HasCard()) {
				continue;
			}

			Vector3 enemyPos = enemy->GetPosition();

			Vector3 enemyDiff = {
				enemyPos.x - pickup.position.x,
				enemyPos.y - pickup.position.y,
				enemyPos.z - pickup.position.z
			};

			float enemyDist = Length(enemyDiff);

			// 敵が拾う
			if (enemyDist < 2.0f) {
				enemy->SetHeldCard(pickup.card);
				pickup.isActive = false;
				break;
			}
		}
	}

	// ==========================================
	// カメラ・各種オブジェクトの更新
	// ==========================================
	// メインカメラの更新（プレイヤーに追従）
	if (camera_) {
		if (debugCamera_ && !debugCamera_->IsActive()) {
			camera_->SetTranslation({
				playerPos_.x,
				playerPos_.y + 15.0f,
				playerPos_.z - 15.0f
				});
			camera_->SetRotation({
				0.9f, 0.0f, 0.0f
				});
		}

		camera_->Update();
	}

	// その他3Dオブジェクトの更新
	for (auto &obj : object3ds_) {
		obj->Update();
	}

	// スプライトの更新
	if (sprite_) {
		sprite_->SetPosition(spritePos_);
		sprite_->Update();
	}

	// テストオブジェクトの更新
	if (testObj_) {
		testObj_->Update();
	}

	// レベル(マップ)・ポストエフェクトの更新
	levelEditor_->Update(playerPos_);
	PostEffect::GetInstance()->Update();

	// 手札(UIなど)の更新
	if (player_ && !player_->IsDead()) {
		handManager_.Update();
	}

	// ==========================================
	// カードシステム用のターゲット検索と更新
	// ==========================================

	Enemy *nearestEnemy = nullptr;
	Vector3 nearestEnemyPos{};
	float nearestEnemyDist = 99999.0f;

	for (auto &enemy : enemies_) {
		if (!enemy || enemy->IsDead()) {
			continue;
		}

		Vector3 pos = enemy->GetPosition();
		Vector3 diff = {
			pos.x - playerPos_.x,
			0.0f,
			pos.z - playerPos_.z
		};

		float dist = Length(diff);
		if (dist < nearestEnemyDist) {
			nearestEnemyDist = dist;
			nearestEnemy = enemy.get();
			nearestEnemyPos = pos;
		}
	}

	Boss *targetBoss = nullptr;
	Vector3 bossPos{};

	if (boss_ && !boss_->IsDead()) {
		targetBoss = boss_.get();
		bossPos = boss_->GetPosition();
	}

	// プレイヤー用カードシステム更新
	if (playerCardSystem_) {
		playerCardSystem_->Update(
			player_.get(),
			nearestEnemy,
			targetBoss,
			playerPos_,
			nearestEnemyPos,
			bossPos,
			levelEditor_->GetLevelData()
		);
	}

	// 敵それぞれのカードシステム更新
	for (size_t i = 0; i < enemies_.size(); ++i) {
		auto &enemy = enemies_[i];
		if (!enemy || enemy->IsDead()) {
			continue;
		}

		if (i < enemyCardSystems_.size() && enemyCardSystems_[i]) {
			enemyCardSystems_[i]->Update(
				player_.get(),
				enemy.get(),
				nullptr,
				playerPos_,
				enemy->GetPosition(),
				Vector3{ 0.0f, 0.0f, 0.0f },
				levelEditor_->GetLevelData()
			);
		}
	}

	// ボス用カードシステム更新
	if (boss_ && !boss_->IsDead() && bossCardSystem_ && levelEditor_ && levelEditor_->IsBossMap()) {
		bossCardSystem_->Update(
			player_.get(),
			nullptr,
			boss_.get(),
			playerPos_,
			Vector3{ 0.0f, 0.0f, 0.0f },
			boss_->GetPosition(),
			levelEditor_->GetLevelData()
		);
	}

	UpdateCardUse(input);
}

void GamePlayScene::Draw() {


	auto dxCommon = DirectXCommon::GetInstance();
	auto commandList = DirectXCommon::GetInstance()->GetCommandList();

	// 画用紙への切り替え
	PostEffect::GetInstance()->PreDrawScene(commandList, dxCommon);


	// 3D描画の前準備
	Obj3dCommon::GetInstance()->PreDraw(commandList);

	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Object3D_CullNone);

	// プレイヤー描画
	if (playerObj_ && player_ && !player_->IsDead() && player_->IsVisible()) {
		playerObj_->Draw(); // 被弾中は点滅表示
	}

	if (bossObj_ && boss_ && !boss_->IsDead() && boss_->IsVisible() && levelEditor_ && levelEditor_->IsBossMap()) {
		bossObj_->Draw();
	}

	for (size_t i = 0; i < enemies_.size(); ++i) {
		auto &enemy = enemies_[i];
		auto &enemyObj = enemyObjs_[i];

		if (!enemy || !enemyObj || enemy->IsDead() || !enemy->IsVisible()) {
			continue;
		}

		enemyObj->Draw();
	}
	// カード使用演出描画
	if (playerCardSystem_) {
		playerCardSystem_->Draw();
	}

	for (auto &system : enemyCardSystems_) {
		if (system) {
			system->Draw();
		}
	}

	if (bossCardSystem_) {
		bossCardSystem_->Draw();
	}

	cardPickupManager_.Draw();


	// 3Dオブジェクト描画
	for (auto &obj : object3ds_) {
		obj->Draw();
	}

	//手札カード
	handManager_.Draw();

	// レベルエディタの描画 
	levelEditor_->Draw(playerPos_);


	// パーティクル描画 (パイプライン切り替え)
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Particle);
	ParticleManager::GetInstance()->Draw(commandList);


	SpriteCommon::GetInstance()->PreDraw(commandList);
	if (sprite_) {
		sprite_->Draw();
	}
	TextManager::GetInstance()->Draw();



	PostEffect::GetInstance()->PostDrawScene(commandList, dxCommon);
	PostEffect::GetInstance()->Draw(commandList, dxCommon);




}

void GamePlayScene::DrawDebugUI() {

#ifdef USE_IMGUI
	// 3Dオブジェクト、カメラ、パーティクルのUI
	Obj3dCommon::GetInstance()->DrawDebugUI();
	if (camera_) { camera_->DrawDebugUI(); }
	if (debugCamera_) { debugCamera_->DrawDebugUI(); }
	ParticleManager::GetInstance()->DrawDebugUI();


	levelEditor_->DrawDebugUI();

	TextManager::GetInstance()->DrawDebugUI();

	ImGui::Begin("Block Dissolve Test");

	// スライダーで 0.0(通常) 〜 1.0(消滅) を操作
	if (ImGui::SliderFloat("ブロックの消滅度", &dissolveThreshold_, 0.0f, 1.0f)) {
		if (testObj_) {
			// スライダーを動かすと、このブロックの閾値だけが書き換わる
			testObj_->SetDissolveThreshold(dissolveThreshold_);
		}
	}

	// 便利なリセットボタン
	if (ImGui::Button("元に戻す")) {
		dissolveThreshold_ = 0.0f;
		if (testObj_) {

			testObj_->SetDissolveThreshold(0.0f);
		}

	}
	ImGui::SameLine();
	if (ImGui::Button("完全に消す")) {
		dissolveThreshold_ = 1.0f;
		if (testObj_) {
			testObj_->SetDissolveThreshold(1.0f);
		}
	}

	ImGui::End();

	ImGui::Begin("Card System Test");

	ImGui::Separator();
	ImGui::Text("[Card Pickups]");
	for (size_t i = 0; i < cardPickupManager_.GetPickups().size(); ++i) {
		const auto &pickup = cardPickupManager_.GetPickups()[i];
		ImGui::Text("%s : pos(%.1f, %.1f, %.1f) active=%s",
			pickup.card.name.c_str(),
			pickup.position.x,
			pickup.position.y,
			pickup.position.z,
			pickup.isActive ? "true" : "false");
	}

	// 仮のプレイヤー状況を表示
	if (player_) {
		ImGui::Text("Player Level: %d", player_->GetLevel());       // プレイヤーレベル表示
		ImGui::Text("Player EXP: %d / %d", player_->GetExp(), player_->GetNextLevelExp()); // 経験値表示
		ImGui::Text("Player Cost: %d / %d", player_->GetCost(), player_->GetMaxCost());     // コスト表示
		ImGui::Text("Player HP: %d / %d", player_->GetHP(), player_->GetMaxHP());           // HP表示
		ImGui::Text("Player Dead: %s", player_->IsDead() ? "true" : "false");               // 死亡状態表示
		ImGui::Text("Player Hit: %s", player_->IsHit() ? "true" : "false");                 // 被弾状態表示
		ImGui::Text("Player Invincible: %s", player_->IsInvincible() ? "true" : "false");   // 無敵状態表示
	}

	if (boss_) {
		ImGui::Separator();
		ImGui::Text("[Boss]");
		ImGui::Text("Boss HP: %d / %d", boss_->GetHP(), boss_->GetMaxHP());
		ImGui::Text("Boss Dead: %s", boss_->IsDead() ? "true" : "false");
	}

	// デバッグ用
	if (ImGui::Button("Add EXP +1")) {
		if (player_) {
			player_->AddExp(1); // デバッグ用経験値追加
		}
	}

	ImGui::Separator();
	ImGui::Text("[Dungeon Floor]");

	ImGui::Text("Current Floor: %d F", currentFloor_); // 現在の階層を表示
	if (ImGui::Button("Go to Next Floor (Stairs)")) {
		AdvanceFloor(); // ボタンを押したら次の階層へ
	}

	// ★修正：図鑑（CardDatabase）からIDを指定して正しいデータを拾う！
	ImGui::SameLine();
	if (ImGui::Button("Pick Up (ID: 2)")) {
		handManager_.AddCard(CardDatabase::GetCardData(2));
	}
	if (ImGui::Button("Pick Up (ID: 3)")) {
		handManager_.AddCard(CardDatabase::GetCardData(3));
	}
	if (ImGui::Button("Pick Up (ID: 4)")) {
		handManager_.AddCard(CardDatabase::GetCardData(4));
	}

	if (ImGui::Button("Pick Up (ID: 5)")) {
		handManager_.AddCard(CardDatabase::GetCardData(5));
	}

	if (ImGui::Button("Pick Up (ID: 7)")) {
		handManager_.AddCard(CardDatabase::GetCardData(7));
	}

	if (ImGui::Button("Pick Up (ID: 8)")) {
		handManager_.AddCard(CardDatabase::GetCardData(8));
	}

	ImGui::Separator();
	ImGui::Text("[Player Hand] : %d/10", handManager_.GetHandSize());

	//手札の数だけループしてボタンを作る
	for (int i = 0; i < handManager_.GetHandSize(); ++i) {
		Card card = handManager_.GetCard(i);

		//ボタンの名前
		std::string btnName = card.name + "(Cost:" + std::to_string(card.cost) + ")##" + std::to_string(i);

		// 使う処理を入れる場合はこのif文の中に書く
		if (ImGui::Button(btnName.c_str())) {
			// 例：手札を使用する処理
		}
	}

	ImGui::End();

#endif

}

void GamePlayScene::ResetBattleDebug() {

	// プレイヤーの状態だけ初期化
	if (player_) {
		player_->Initialize();
		playerScale_ = { 1.0f, 1.0f, 1.0f };
	}

	if (boss_) {
		boss_->Initialize();
		boss_->SetScale({ 2.0f, 2.0f, 2.0f });
	}

	bossDeadHandled_ = false;

	// カード使用システムの状態をリセット
	if (playerCardSystem_) {
		playerCardSystem_->Reset();
	}

	for (auto &system : enemyCardSystems_) {
		if (system) {
			system->Reset();
		}
	}

	if (bossCardSystem_) {
		bossCardSystem_->Reset();
	}
	// 手札を初期化
	handManager_.Initialize(uiCamera_.get(), textures_["noise0"].srvIndex);
	handManager_.AddCard(CardDatabase::GetCardData(1));

	// ダンジョン生成 + プレイヤー再配置 + 敵/カード再生成 + ボス再配置
	RegenerateDungeonAndRespawnPlayer(5);

	// 交換モードも戻しておく
	isCardSwapMode_ = false;
	pendingCard_ = Card{};
}


void GamePlayScene::UpdateCardSwapMode(Input *input) {

	// 手札の選択と見た目の更新
	handManager_.Update();
	uiCamera_->Update(); // UIカメラの更新もここで行う

	if (input->Triggerkey(DIK_SPACE)) {
		// 現在選んでいるカードを取得
		Card selectedCard = handManager_.GetSelectedCard();

		// 選んでいるカードがID: 1（初期カード)なら交換をしない
		if (selectedCard.id == 1) {
			return;
		}

		handManager_.SwapSelectedCard(pendingCard_);
		isCardSwapMode_ = false;
	} else if (input->Triggerkey(DIK_C)) {
		isCardSwapMode_ = false;
	}
}

void GamePlayScene::UpdateCardUse(Input* input) {

	if (!player_ || player_->IsDead() || !input->Triggerkey(DIK_SPACE)) {
		return;
	}

	if (player_->IsDodging()) {
		return;
	}

	if (player_->IsActionLocked()) {
		return;
	}

	if (handManager_.IsSelectedCardDissolving()) {
		return;
	}

	Card selectedCard = handManager_.GetSelectedCard();
	if (selectedCard.id == -1) {
		return;
	}

	if (!player_->CanUseCost(selectedCard.cost)) {
		return;
	}

	player_->UseCost(selectedCard.cost);

	if (playerCardSystem_) {
		playerCardSystem_->UseCard(
			selectedCard,
			playerPos_,
			player_->GetRotation().y,
			true,
			player_.get()
		);
	}

	if (selectedCard.id != 1) {
		handManager_.StartDissolveSelectedCard();
	}
}

GamePlayScene::GamePlayScene() {}

GamePlayScene::~GamePlayScene() {}
// 終了
void GamePlayScene::Finalize() {

	object3ds_.clear();

	playerCardSystem_.reset();
	enemyCardSystems_.clear();
	bossCardSystem_.reset();

	TextManager::GetInstance()->Finalize();

	textures_.clear();
	depthStencilResource_.Reset();
}

void GamePlayScene::SpawnEnemiesRandom(int enemyCount, int margin) {

	enemies_.clear();
	enemyObjs_.clear();
	enemyDeadHandled_.clear();
	enemyCardSystems_.clear();

	if (!spawnManager_.HasLevelData()) {
		return;
	}

	std::vector<std::pair<int, int>> candidates =
		spawnManager_.FindEnemySpawnCandidates(margin);

	if (candidates.empty()) {
		return;
	}

	std::random_device rd;
	std::mt19937 mt(rd());
	std::shuffle(candidates.begin(), candidates.end(), mt);

	int spawnCount = std::min(enemyCount, static_cast<int>(candidates.size()));

	for (int i = 0; i < spawnCount; ++i) {
		int tileX = candidates[i].first;
		int tileZ = candidates[i].second;

		Vector3 worldPos = spawnManager_.TileToWorldPosition(tileX, tileZ, 0.0f);

		auto enemy = std::make_unique<Enemy>();
		enemy->Initialize();
		enemy->SetPosition(worldPos);
		enemy->SetScale({ 1.0f, 1.0f, 1.0f });

		auto enemyObj = std::unique_ptr<Obj3d>(Obj3d::Create("sphere"));
		if (enemyObj) {
			enemyObj->SetCamera(camera_.get());
			enemyObj->SetTranslation(worldPos);
			enemyObj->SetScale({ 1.0f, 1.0f, 1.0f });
			enemyObj->Update();
		}

		auto enemyCardSystem = std::make_unique<CardUseSystem>();
		enemyCardSystem->Initialize(camera_.get());

		enemies_.push_back(std::move(enemy));
		enemyObjs_.push_back(std::move(enemyObj));
		enemyDeadHandled_.push_back(false);
		enemyCardSystems_.push_back(std::move(enemyCardSystem));
	}
}

void GamePlayScene::SpawnCardsRandom(int cardCount, int margin) {

	if (!spawnManager_.HasLevelData()) {
		return;
	}

	cardPickupManager_.Initialize(camera_.get());

	std::vector<std::pair<int, int>> candidates =
		spawnManager_.FindCardSpawnCandidates(margin);

	if (candidates.empty()) {
		return;
	}

	std::random_device rd;
	std::mt19937 mt(rd());
	std::shuffle(candidates.begin(), candidates.end(), mt);

	int spawnCount = std::min(cardCount, static_cast<int>(candidates.size()));

	// CSVのカード種類数を取得して範囲に設定
	int maxCardId = static_cast<int>(CardDatabase::GetCardCount());
	std::uniform_int_distribution<int> cardDist(2, maxCardId);

	for (int i = 0; i < spawnCount; ++i) {
		int tileX = candidates[i].first;
		int tileZ = candidates[i].second;

		Vector3 worldPos = spawnManager_.TileToWorldPosition(tileX, tileZ, 0.0f);

		// csvのID範囲からランダムにカードを選ぶ
		int cardId = cardDist(mt);

		cardPickupManager_.AddPickup(worldPos, CardDatabase::GetCardData(cardId));
	}
}

void GamePlayScene::RespawnPlayerInRoom() {
	if (!levelEditor_ || !player_) {
		return;
	}

	if (levelEditor_->IsBossMap()) {
		Vector3 center = levelEditor_->GetMapCenterPosition(1.5f);
		playerPos_ = center;
		playerPos_.z -= 6.0f; // ボスより少し手前
	} else {
		playerPos_ = levelEditor_->GetRandomPlayerSpawnPosition(1.5f);
	}

	playerScale_ = { 1.0f, 1.0f, 1.0f };

	player_->SetPosition(playerPos_);
	player_->SetScale(playerScale_);

	if (playerObj_) {
		playerObj_->SetTranslation(playerPos_);
		playerObj_->SetRotation({ 0.0f, 0.0f, 0.0f });
		playerObj_->SetScale(playerScale_);
		playerObj_->Update();
	}

	if (camera_) {
		camera_->SetTranslation({
			playerPos_.x,
			playerPos_.y + 15.0f,
			playerPos_.z - 15.0f
			});
		camera_->SetRotation({
			0.9f, 0.0f, 0.0f
			});
		camera_->Update();
	}
}

void GamePlayScene::RegenerateDungeonAndRespawnPlayer(int roomCount) {
	if (!levelEditor_) {
		return;
	}

	if (levelEditor_) {
		// ★ボス部屋ではない（通常部屋の）時だけ、ランダムダンジョンを生成する
		if (!levelEditor_->IsBossMap()) {
			levelEditor_->GenerateRandomDungeon(roomCount);
		}
	}

	// プレイヤー再配置
	RespawnPlayerInRoom();

	// スポーンマネージャに新しいマップを渡し直す
	spawnManager_.SetLevelData(&levelEditor_->GetLevelData());

	if (levelEditor_->IsBossMap()) {
		// bossマップでは雑魚敵とカードを消す
		ClearEnemiesAndCards();
	} else {
		// 通常マップではいつも通り生成
		SpawnEnemiesRandom(enemySpawnCount_, enemySpawnMargin_);
		SpawnCardsRandom(cardSpawnCount_, cardSpawnMargin_);
	}

	// ボス再配置
	RespawnBossInRoom();
}

void GamePlayScene::RespawnBossInRoom() {
	if (!levelEditor_ || !boss_) {
		return;
	}

	// 通常マップではボスを出さない
	if (!levelEditor_->IsBossMap()) {
		boss_->Initialize();
		bossDeadHandled_ = false;

		// 画面外に逃がしておく
		boss_->SetPosition({ 9999.0f, -9999.0f, 9999.0f });

		if (bossObj_) {
			bossObj_->SetTranslation({ 9999.0f, -9999.0f, 9999.0f });
			bossObj_->Update();
		}
		return;
	}

	// bossマップでは中央固定
	Vector3 spawnPos = levelEditor_->GetMapCenterPosition(2.0f);

	boss_->Initialize();
	boss_->SetPosition(spawnPos);
	boss_->SetScale({ 2.0f, 2.0f, 2.0f });
	bossDeadHandled_ = false;

	if (bossObj_) {
		bossObj_->SetTranslation(boss_->GetPosition());
		bossObj_->SetRotation(boss_->GetRotation());
		bossObj_->SetScale(boss_->GetScale());
		bossObj_->Update();
	}
}

// 敵とカードを完全にクリアしてスポーンマネージャもリセット
void GamePlayScene::ClearEnemiesAndCards() {
	enemies_.clear();
	enemyObjs_.clear();
	enemyDeadHandled_.clear();
	enemyCardSystems_.clear();

	cardPickupManager_.Initialize(camera_.get());
}
void GamePlayScene::AdvanceFloor() {
	currentFloor_++; // 階層を1つ進める

	if (levelEditor_) {
		// 5の倍数階ならボス部屋、それ以外は通常部屋
		if (currentFloor_ % 5 == 0) {
			levelEditor_->ChangeToBossMap();
		} else {
			levelEditor_->ChangeToNormalMap();
		}
	}

	// 最後に、マップが変わったのでプレイヤーや敵をリセット・再配置する
	// ※ 既存の ResetBattleDebug() が全てやってくれるはずです
	ResetBattleDebug();
}
