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
#include "game/enemy/Enemy.h"
#include "game/card/CardUseSystem.h"


using namespace VectorMath;
using namespace MatrixMath;
// 初期化
void GamePlayScene::Initialize() {
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	WindowProc* windowProc = WindowProc::GetInstance();

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

	// カード使用システム初期化
	cardUseSystem_ = std::make_unique<CardUseSystem>();
	cardUseSystem_->Initialize(camera_.get());

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


	// マップエディタ生成・初期化
	// マップエディタ生成・初期化
	levelEditor_ = std::make_unique<LevelEditor>();
	levelEditor_->SetCamera(camera_.get());
	levelEditor_->Initialize();

	// これだけでOK
	RegenerateDungeonAndRespawnPlayer(8);

	// カード用の3Dモデルを読み込んでおく（※パスやファイル名はご自身の環境に合わせてください）
	ModelManager::GetInstance()->LoadModel("plane", "resources/plane", "plane.obj");

	// CSVからカードデータベースを初期化
	CardDatabase::Initialize("Resources/CardData.csv");

	// 手札マネージャーの初期化
	handManager_.Initialize(uiCamera_.get());

	//最初から手札にID１を追加する
	handManager_.AddCard(CardDatabase::GetCardData(1));


	SpawnCardsRandom(cardSpawnCount_, cardSpawnMargin_);

	//enemyDeadHandled_ = false; // 敵死亡処理フラグ初期化
}

void GamePlayScene::Update() {

	// デバッグカメラ更新
	if (debugCamera_) {
		debugCamera_->Update(camera_.get());
	}

	Input* input = Input::GetInstance();

	if (isCardSwapMode_) {
		UpdateCardSwapMode(input);
		return;
	}

	// デバッグ用リセット
	if (input->Triggerkey(DIK_R)) {
		ResetBattleDebug(); // プレイヤー・敵・カードを初期状態に戻す
	}

	// BGM再生 (シングルトン)
	if (input->Triggerkey(DIK_SPACE)) {
		AudioManager::GetInstance()->PlayWave(bgmFile_);
	}
	// タイトルシーンへ移動
	if (input->Triggerkey(DIK_T)) {
		SceneManager::GetInstance()->ChangeScene(std::make_unique<TitleScene>());
	}
	// パーティクル発生 (シングルトン)
	if (input->Triggerkey(DIK_P)) {
		ParticleManager::GetInstance()->Emit("Circle", { 0.0f, 0.0f, 0.0f }, 10);
	}
	// パーティクル更新
	ParticleManager::GetInstance()->Update(camera_.get());



	if (player_) {
		// デバッグカメラが有効ならプレイヤーの入力を切る
		if (debugCamera_ && debugCamera_->IsActive()) {
			player_->SetInputEnable(false);
		} else {
			player_->SetInputEnable(true);
		}


		Vector3 oldPos = player_->GetPosition();

		// プレイヤー更新
		player_->Update();
		playerPos_ = player_->GetPosition();

		// プレイヤーのAABBを作成
		AABB playerAABB;
		playerAABB.min = { playerPos_.x - 0.5f, playerPos_.y - 0.5f, playerPos_.z - 0.5f };
		playerAABB.max = { playerPos_.x + 0.5f, playerPos_.y + 0.5f, playerPos_.z + 0.5f };


		const LevelData& level = levelEditor_->GetLevelData();

		// --- ここからプレイヤーの当たり判定改良 ---
		// プレイヤーの現在座標から、今いるマスの配列インデックス(x, z)を割り出す
		int centerGridX = static_cast<int>(std::round(playerPos_.x / level.tileSize));
		int centerGridZ = static_cast<int>(std::round(playerPos_.z / level.tileSize));

		// 調べる範囲を「周囲1マス（3x3 = 9マス）」に限定する
		// （※マップ外のマイナスや、width以上にならないように std::max/min で制限）
		int startX = std::max(0, centerGridX - 1);
		int endX = std::min(level.width - 1, centerGridX + 1);
		int startZ = std::max(0, centerGridZ - 1);
		int endZ = std::min(level.height - 1, centerGridZ + 1);

		bool isPlayerHit = false; // 当たったかどうかを判定するフラグ

		// 0からではなく、計算した範囲(start〜end)だけをループする！
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
					isPlayerHit = true; // フラグを立てて二重ループをまとめて抜ける
					break;
				}
			}
		}
		// --- 改良ここまで ---

		playerScale_ = player_->GetScale();
	}

	if (playerObj_) {
		playerObj_->SetTranslation(playerPos_);
		playerObj_->SetRotation(player_->GetRotation());
		playerObj_->SetScale(playerScale_);
		playerObj_->Update();
	}


	for (size_t i = 0; i < enemies_.size(); ++i) {
		auto& enemy = enemies_[i];
		if (!enemy || enemy->IsDead()) {
			continue;
		}

		Vector3 oldEnemyPos = enemy->GetPosition();

		enemy->SetPlayerPosition(playerPos_);

		bool foundCard = false;
		Vector3 nearestCardPos{};
		float nearestCardDist = 99999.0f;

		for (auto& pickup : cardPickupManager_.GetPickups()) {
			if (!pickup.isActive) {
				continue;
			}

			Vector3 diff = {
				pickup.position.x - oldEnemyPos.x,
				0.0f,
				pickup.position.z - oldEnemyPos.z
			};

			float dist = Length(diff);

			if (dist < 6.0f && dist < nearestCardDist) {
				nearestCardDist = dist;
				nearestCardPos = pickup.position;
				foundCard = true;
			}
		}

		enemy->SetCardTarget(foundCard, nearestCardPos);

		enemy->Update();

		for (size_t i = 0; i < enemies_.size(); ++i) {
			auto& enemy = enemies_[i];
			auto& enemyObj = enemyObjs_[i];

			if (!enemy || !enemyObj || enemy->IsDead()) {
				continue;
			}

			enemyObj->SetTranslation(enemy->GetPosition());
			enemyObj->SetRotation(enemy->GetRotation());
			enemyObj->SetScale(enemy->GetScale());
			enemyObj->Update();
		}

		Vector3 enemyPos = enemy->GetPosition();

		AABB enemyAABB;
		enemyAABB.min = { enemyPos.x - 0.5f, enemyPos.y - 0.5f, enemyPos.z - 0.5f };
		enemyAABB.max = { enemyPos.x + 0.5f, enemyPos.y + 0.5f, enemyPos.z + 0.5f };

		const LevelData& level = levelEditor_->GetLevelData();

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
		auto& enemy = enemies_[i];
		if (!enemy || !player_ || enemy->IsDead() || player_->IsDead()) {
			continue;
		}

		Vector3 enemyPos = enemy->GetPosition();

		if (enemy->GetAttackRequest()) {
			Vector3 diff = {
				playerPos_.x - enemyPos.x,
				0.0f,
				playerPos_.z - enemyPos.z
			};

			if (Length(diff) <= 2.0f) {
				player_->TakeDamage(1, enemyPos);
			}

			enemy->ClearAttackRequest();
		}

		if (enemy->GetCardUseRequest()) {
			if (enemy->HasCard()) {
				if (cardUseSystem_) {
					cardUseSystem_->UseCard(
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

	for (size_t i = 0; i < enemies_.size(); ++i) {
		auto& enemy = enemies_[i];
		if (!enemy) {
			continue;
		}

		if (enemy->IsDead() && !enemyDeadHandled_[i]) {
			if (player_) {
				player_->AddExp(1);
			}

			if (enemy->HasCard()) {
				cardPickupManager_.AddPickup(enemy->GetPosition(), enemy->GetHeldCard());
				enemy->ClearHeldCard();
			}

			enemyDeadHandled_[i] = true;
		}
	}

	cardPickupManager_.Update();

	for (auto& pickup : cardPickupManager_.GetPickups()) {
		if (!pickup.isActive) {
			continue;
		}

		// プレイヤーが拾う
		Vector3 playerDiff = {
			playerPos_.x - pickup.position.x,
			playerPos_.y - pickup.position.y,
			playerPos_.z - pickup.position.z
		};

		float playerDist = Length(playerDiff);

		// プレイヤーが拾う
		if (player_ && !player_->IsDead() && playerDist < 2.0f) {
			bool success = handManager_.AddCard(pickup.card);
			if (success) {
				pickup.isActive = false;
				continue;
			} else {
				isCardSwapMode_ = true;
				pendingCard_ = pickup.card; // 拾おうとしたカードを一時保存

				// 一旦マップ上からは消しておく
				pickup.isActive = false;
				continue;
			}
		}

		// 敵が拾う
		for (size_t i = 0; i < enemies_.size(); ++i) {
			auto& enemy = enemies_[i];
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

			if (enemyDist < 2.0f) {
				enemy->SetHeldCard(pickup.card);
				pickup.isActive = false;
				break;
			}
		}
	}

	if (camera_) {
		// デバッグカメラが非アクティブなときは、プレイヤーを追従する通常カメラ
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

		// 行列の更新自体は常に行う
		camera_->Update();
	}

	// 全オブジェクト更新
	for (auto& obj : object3ds_) {
		obj->Update();
	}
	// スプライト更新
	sprite_->Update();


	Sphere playerCollider;
	playerCollider.center = playerPos_;
	playerCollider.radius = playerScale_.x;




	if (sprite_) {
		sprite_->SetPosition(spritePos_);
		sprite_->Update();
	}
	if (testObj_) {
		testObj_->Update();
	}

	if ( sprite_ ) {
		sprite_->Update();
	}

	levelEditor_->Update(playerPos_);
	PostEffect::GetInstance()->Update();

	//levelEditor_->Update();

	// 手札（3Dモデル）の移動などの更新
	if (player_ && !player_->IsDead()) {
		handManager_.Update();
	}

	// カード使用システム更新
	Enemy* nearestEnemy = nullptr;
	Vector3 nearestEnemyPos{};
	float nearestEnemyDist = 99999.0f;

	for (auto& enemy : enemies_) {
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

	if (cardUseSystem_) {
		cardUseSystem_->Update(
			player_.get(),
			nearestEnemy,
			playerPos_,
			nearestEnemyPos,
			levelEditor_->GetLevelData()
		);
	}

	// カード使用
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

	for (size_t i = 0; i < enemies_.size(); ++i) {
		auto& enemy = enemies_[i];
		auto& enemyObj = enemyObjs_[i];

		if (!enemy || !enemyObj || enemy->IsDead() || !enemy->IsVisible()) {
			continue;
		}

		enemyObj->Draw();
	}
	// カード使用演出描画
	if (cardUseSystem_) {
		cardUseSystem_->Draw();
	}
	cardPickupManager_.Draw();


	// 3Dオブジェクト描画
	for (auto& obj : object3ds_) {
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
	if ( sprite_ ) {
		sprite_->Draw();
	}


	PostEffect::GetInstance()->PostDrawScene(commandList, dxCommon);
	PostEffect::GetInstance()->Draw(commandList,dxCommon);

}

void GamePlayScene::DrawDebugUI() {

#ifdef USE_IMGUI
	// 3Dオブジェクト、カメラ、パーティクルのUI
	Obj3dCommon::GetInstance()->DrawDebugUI();
	if (camera_) { camera_->DrawDebugUI(); }
	if (debugCamera_) { debugCamera_->DrawDebugUI(); }
	ParticleManager::GetInstance()->DrawDebugUI();


	levelEditor_->DrawDebugUI();

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
		const auto& pickup = cardPickupManager_.GetPickups()[i];
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

	// デバッグ用
	if (ImGui::Button("Add EXP +1")) {
		if (player_) {
			player_->AddExp(1); // デバッグ用経験値追加
		}
	}

	ImGui::Separator();
	ImGui::Text("[Dungeon Floor]");

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

	if (ImGui::Button("Pick Up (ID: 6)")) {
		handManager_.AddCard(CardDatabase::GetCardData(6));
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

	// カード使用システムの状態をリセット
	if (cardUseSystem_) {
		cardUseSystem_->Reset();
	}

	// 手札を初期化
	handManager_.Initialize(uiCamera_.get());
	handManager_.AddCard(CardDatabase::GetCardData(1));

	// ダンジョン生成 + プレイヤー再配置 + 敵/カード再生成
	RegenerateDungeonAndRespawnPlayer(5);

	// 交換モードも戻しておく
	isCardSwapMode_ = false;
	pendingCard_ = Card{};
}
void GamePlayScene::UpdateCardSwapMode(Input* input) {

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

	// プレイヤーが死んでいる、またはスペースキーが押されていなければ何もしない
	if (!player_ || player_->IsDead() || !input->Triggerkey(DIK_SPACE)) {
		return;
	}

	// 選んでいるカードを取得（エラーカードなら何もしない）
	Card selectedCard = handManager_.GetSelectedCard();
	if (selectedCard.id == -1) {
		return;
	}

	// コストが足りなければ何もしない
	if (!player_->CanUseCost(selectedCard.cost)) {
		return;
	}

	// ----------------------------------------
	// ここから下が実際の「使用処理」
	// ----------------------------------------

	// コスト消費
	player_->UseCost(selectedCard.cost);

	// カード効果発動
	if (cardUseSystem_) {
		cardUseSystem_->UseCard(
			selectedCard,
			playerPos_,
			player_->GetRotation().y,
			true,
			player_.get()
		);
	}

	// 使い切りカード（IDが1以外）なら手札から削除
	if (selectedCard.id != 1) {
		handManager_.RemoveSelectedCard();
	}
}

GamePlayScene::GamePlayScene() {}

GamePlayScene::~GamePlayScene() {}
// 終了
void GamePlayScene::Finalize() {


	object3ds_.clear();

	textures_.clear();
	depthStencilResource_.Reset();
}

void GamePlayScene::SpawnEnemiesRandom(int enemyCount, int margin) {

	enemies_.clear();
	enemyObjs_.clear();
	enemyDeadHandled_.clear();

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

		enemies_.push_back(std::move(enemy));
		enemyObjs_.push_back(std::move(enemyObj));
		enemyDeadHandled_.push_back(false);
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

	for (int i = 0; i < spawnCount; ++i) {
		int tileX = candidates[i].first;
		int tileZ = candidates[i].second;

		Vector3 worldPos = spawnManager_.TileToWorldPosition(tileX, tileZ, 0.0f);

		// 今は仮で ID:2 と ID:3 をランダムに出す
		int cardId = (i % 2 == 0) ? 2 : 3;

		cardPickupManager_.AddPickup(worldPos, CardDatabase::GetCardData(cardId));
	}
}

void GamePlayScene::RespawnPlayerInRoom() {
	if (!levelEditor_ || !player_) {
		return;
	}

	playerPos_ = levelEditor_->GetRandomPlayerSpawnPosition(2.0f);
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

	levelEditor_->GenerateRandomDungeon(roomCount);

	// プレイヤー再配置
	RespawnPlayerInRoom();

	// スポーンマネージャに新しいマップを渡し直す
	spawnManager_.SetLevelData(&levelEditor_->GetLevelData());

	// 敵やカードも新しいダンジョンに合わせて再配置したいならここで再生成
	SpawnEnemiesRandom(enemySpawnCount_, enemySpawnMargin_);
	SpawnCardsRandom(cardSpawnCount_, cardSpawnMargin_);
}