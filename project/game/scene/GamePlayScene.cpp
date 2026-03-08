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
void GamePlayScene::Initialize(){
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	WindowProc* windowProc = WindowProc::GetInstance();

	// コマンドリスト取得
	auto commandList = dxCommon->GetCommandList();

	// BGMロード (シングルトン)
	AudioManager::GetInstance()->LoadWave(bgmFile_);
	// モデル読み込み (シングルトン)
	ModelManager::GetInstance()->LoadModel("fence", "resources", "fence.obj");
	
	ModelManager::GetInstance()->LoadModel("grass", "resources", "terrain.obj");
	ModelManager::GetInstance()->LoadModel("block", "resources/block","block.obj");
	
	// 球モデル作成 (シングルトン)
	ModelManager::GetInstance()->CreateSphereModel("sphere", 16);
	// パーティクルグループ作成 (シングルトン)
	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "resources/uvChecker.png");

	// テクスチャ読み込み
	textures_["uvChecker"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/uvChecker.png", commandList);
	textures_["monsterBall"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/monsterBall.png", commandList);
	textures_["fence"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/fence.png", commandList);
	textures_["circle"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/circle.png", commandList);
	
	// カメラ生成
	camera_ = std::make_unique<Camera>(windowProc->GetClientWidth(), windowProc->GetClientHeight(), dxCommon);
	camera_->SetTranslation({ 0.0f, 2.0f, -15.0f });

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

	enemyObj_ = Obj3d::Create("sphere");
	if (enemyObj_) {
		enemyObj_->SetCamera(camera_.get());
		enemyObj_->SetTranslation(enemyPos_);
		enemyObj_->SetScale(enemyScale_);
	}

	enemy_ = std::make_unique<Enemy>();
	enemy_->Initialize();
	enemy_->SetPosition(enemyPos_);
	enemy_->SetScale(enemyScale_);

	// カード使用システム初期化
	cardUseSystem_ = std::make_unique<CardUseSystem>();
	cardUseSystem_->Initialize(camera_.get());

	// ファイル名を指定するだけで、読み込み・生成・配置
	// 引数: (ファイルパス, 座標)
	sprite_ = Sprite::Create(textures_["uvChecker"].srvIndex, spritePos_);

	// スプライトの切り抜き範囲を設定 (テクスチャの左上から128x128ピクセルを使用)
	sprite_->SetTextureRect(0.0f, 0.0f, 128.0f, 128.0f, 256.0f, 256.0f);


	// デプスステンシル作成 (TextureManagerシングルトン)
	depthStencilResource_ = TextureManager::GetInstance()->CreateDepthStencilTextureResource(
		windowProc->GetClientWidth(), windowProc->GetClientHeight()
	);

	

	levelEditor_ = std::make_unique<LevelEditor>();
	levelEditor_->SetCamera(camera_.get());
	levelEditor_->Initialize();

	// カード用の3Dモデルを読み込んでおく（※パスやファイル名はご自身の環境に合わせてください）
	ModelManager::GetInstance()->LoadModel("plane", "resources/plane", "plane.obj");

	// CSVからカードデータベースを初期化
	CardDatabase::Initialize("Resources/CardData.csv");

	// 手札マネージャーの初期化
	handManager_.Initialize(camera_.get());

	//最初から手札にID１を追加する
	handManager_.AddCard(CardDatabase::GetCardData(1));


	cardPickupManager_.Initialize(camera_.get());

	cardPickupManager_.AddPickup({ 3.0f, 0.0f, 3.0f }, CardDatabase::GetCardData(2));
	cardPickupManager_.AddPickup({ -3.0f, 0.0f, 5.0f }, CardDatabase::GetCardData(3));

	enemyDeadHandled_ = false; // 敵死亡処理フラグ初期化

}

void GamePlayScene::Update(){
	
	// デバッグカメラ更新
	if (debugCamera_) {
		debugCamera_->Update(camera_.get());
	}

	Input* input = Input::GetInstance();

	// デバッグ用リセット
	if (input->Triggerkey(DIK_R)) {
		ResetBattleDebug(); // プレイヤー・敵・カードを初期状態に戻す
	}

	// BGM再生 (シングルトン)
	if ( input->Triggerkey(DIK_SPACE) ) {
		AudioManager::GetInstance()->PlayWave(bgmFile_);
	}
	// タイトルシーンへ移動
	if ( input->Triggerkey(DIK_T) ) {
		SceneManager::GetInstance()->ChangeScene(std::make_unique<TitleScene>());
	}
	// パーティクル発生 (シングルトン)
	if ( input->Triggerkey(DIK_P) ) {
		ParticleManager::GetInstance()->Emit("Circle", { 0.0f, 0.0f, 0.0f }, 10);
	}
	// パーティクル更新
	ParticleManager::GetInstance()->Update(camera_.get());

	if (player_) {
		Vector3 oldPos = player_->GetPosition();

		// プレイヤー更新
		player_->Update();
		playerPos_ = player_->GetPosition();

		// プレイヤーのAABBを作成
		AABB playerAABB;
		playerAABB.min = { playerPos_.x - 0.5f, playerPos_.y - 0.5f, playerPos_.z - 0.5f };
		playerAABB.max = { playerPos_.x + 0.5f, playerPos_.y + 0.5f, playerPos_.z + 0.5f };

		for (const auto& obj : levelEditor_->GetLevelData().objects) {

			if (obj.type != "block") continue;

			AABB blockAABB;
			blockAABB.min = { obj.translation.x - 1.0f, obj.translation.y - 1.0f, obj.translation.z - 1.0f };
			blockAABB.max = { obj.translation.x + 1.0f, obj.translation.y + 1.0f, obj.translation.z + 1.0f };

			if (Collision::IsCollision(playerAABB, blockAABB)) {
				player_->SetPosition(oldPos);
				playerPos_ = oldPos;
				break;
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


	if (enemy_ && !enemy_->IsDead()) {
		enemy_->SetPlayerPosition(playerPos_); // プレイヤー位置を渡す

		bool foundCard = false;                // 近くに拾えるカードがあるか
		Vector3 nearestCardPos{};              // 一番近いカード位置
		Card nearestCard{};                    // 一番近いカード情報
		float nearestCardDist = 99999.0f;      // 一番近いカード距離

		for (auto& pickup : cardPickupManager_.GetPickups()) {
			if (!pickup.isActive) { continue; } // 非アクティブカードは無視

			Vector3 diff = {
				pickup.position.x - enemyPos_.x,
				0.0f,
				pickup.position.z - enemyPos_.z
			};

			float dist = Length(diff); // 敵とカードの距離

			if (dist < 6.0f && dist < nearestCardDist) {
				nearestCardDist = dist;     // 最短距離更新
				nearestCardPos = pickup.position;
				nearestCard = pickup.card;
				foundCard = true;
			}
		}

		Vector3 toPlayer = {
			playerPos_.x - enemyPos_.x,
			0.0f,
			playerPos_.z - enemyPos_.z
		};

		float playerDist = Length(toPlayer); // 敵とプレイヤーの距離

		// 行動ロック中でなければ状態を決める
		if (!enemy_->IsActionLocked()) {

			if (enemy_->HasCard()) { // カードを持っている場合

				if (playerDist <= 6.0f) {
					enemy_->SetState(Enemy::State::UseCard); // 射程内ならカード使用
				} else {
					enemy_->SetState(Enemy::State::ChasePlayer); // 射程外なら追跡
				}

			} else { // カードを持っていない場合

				if (playerDist <= 2.0f) {
					enemy_->SetState(Enemy::State::AttackPlayer); // 近ければ通常攻撃
				} else if (foundCard) {
					enemy_->SetTargetCardPosition(nearestCardPos); // カード位置を設定
					enemy_->SetState(Enemy::State::MoveToCard);    // カード回収を優先
				} else if (playerDist <= 8.0f) {
					enemy_->SetState(Enemy::State::ChasePlayer);   // プレイヤー追跡
				} else {
					enemy_->SetState(Enemy::State::Patrol);        // 何もなければ巡回
				}
			}
		}

		enemy_->Update();                     // 敵更新
		enemyPos_ = enemy_->GetPosition();    // 位置反映
		enemyScale_ = enemy_->GetScale();     // スケール反映
	}

	// 敵の攻撃結果をプレイヤーへ反映
	if (enemy_ && player_ && !enemy_->IsDead() && !player_->IsDead()) {

		// 近接攻撃リクエストが来ていたらプレイヤーにダメージ
		if (enemy_->GetAttackRequest()) {

			Vector3 diff = {
				playerPos_.x - enemyPos_.x,
				0.0f,
				playerPos_.z - enemyPos_.z
			}; // 敵とプレイヤーの距離ベクトル

			// 念のため近接範囲内のときだけダメージ
			if (Length(diff) <= 2.0f) {
				player_->TakeDamage(1, enemyPos_); // 敵位置を攻撃元として渡す
			}

			enemy_->ClearAttackRequest(); // 攻撃リクエスト消去
		}

		// カード使用リクエストが来ていたら敵の所持カードを発動
		if (enemy_->GetCardUseRequest()) {

			// カードを持っている場合のみ使用
			if (enemy_->HasCard()) {

				if (cardUseSystem_) {
					cardUseSystem_->UseCard(
						enemy_->GetHeldCard(),      // 使用するカード
						enemyPos_,                  // 使用者位置
						enemy_->GetRotation().y,    // 使用者の向き
						false                       // 敵が使用
					);
				}

				enemy_->ClearHeldCard(); // 使用後はカードを消費
			}

			enemy_->ClearCardUseRequest(); // カード使用リクエスト消去
		}
	}

	if (enemy_ && enemy_->IsDead() && !enemyDeadHandled_) {

		if (player_) {
			player_->AddExp(1); // 敵撃破で経験値を加算
		}

		if (enemy_->HasCard()) {
			cardPickupManager_.AddPickup(enemyPos_, enemy_->GetHeldCard()); // 敵の位置にカードを落とす
			enemy_->ClearHeldCard(); // 所持カードを空にする
		}

		enemyDeadHandled_ = true; // 死亡時処理は1回だけ
	}

	if (enemyObj_ && enemy_ && !enemy_->IsDead()) {
		enemyObj_->SetTranslation(enemyPos_);      // 敵位置反映
		enemyObj_->SetRotation(enemy_->GetRotation()); // 敵回転反映
		enemyObj_->SetScale(enemyScale_);          // 敵スケール反映
		enemyObj_->Update();                       // 行列更新
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
			}
		}

		// 敵が拾う
		if (enemy_ && !enemy_->IsDead() && !enemy_->HasCard()) {
			Vector3 enemyDiff = {
				enemyPos_.x - pickup.position.x,
				enemyPos_.y - pickup.position.y,
				enemyPos_.z - pickup.position.z
			};

			float enemyDist = Length(enemyDiff);

			if (enemyDist < 2.0f) {
				enemy_->SetHeldCard(pickup.card);
				pickup.isActive = false;
			}
		}
	}

	if (camera_) {
		camera_->SetTranslation({
			playerPos_.x,
			playerPos_.y + 8.0f,
			playerPos_.z - 18.0f
			});
		camera_->SetRotation({
			0.45f, 0.0f, 0.0f
			});
		camera_->Update();
	}

	// 全オブジェクト更新
	for ( auto& obj : object3ds_ ) {
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


	levelEditor_->Update();

	// 手札（3Dモデル）の移動などの更新
	if (player_ && !player_->IsDead()) {
		handManager_.Update();
	}

	// カード使用システム更新
	if (cardUseSystem_) {
		cardUseSystem_->Update(
			player_.get(), // プレイヤー
			enemy_.get(),  // 敵
			playerPos_,    // プレイヤー位置
			enemyPos_      // 敵位置
		);
	}

	// カード使用
	if (input->Triggerkey(DIK_SPACE) && player_ && !player_->IsDead()) {

		Card selectedCard = handManager_.GetSelectedCard(); // 現在選択中のカード取得

		if (selectedCard.id != -1) {

			// コストが足りる場合のみ使用
			if (player_->CanUseCost(selectedCard.cost)) {

				player_->UseCost(selectedCard.cost); // コスト消費

				// カード使用システムにカード使用を依頼
				if (cardUseSystem_) {
					cardUseSystem_->UseCard(
						selectedCard,             // 使用するカード
						playerPos_,               // 使用者位置
						player_->GetRotation().y, // 使用者の向き
						true,                     // プレイヤーが使用
						player_.get()             // プレイヤー本体
					);
				}

				// 初期カード以外は使用後に削除
				if (selectedCard.id != 1) {
					handManager_.RemoveSelectedCard(); // 使い切りカードは手札から削除
				}
			}
		}
	}

}

void GamePlayScene::Draw(){


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

	if (enemyObj_ && enemy_ && !enemy_->IsDead() && enemy_->IsVisible()) {
		enemyObj_->Draw(); // ヒット中は点滅表示
	}

	// カード使用演出描画
	if (cardUseSystem_) {
		cardUseSystem_->Draw();
	}
	cardPickupManager_.Draw();

	
	// 3Dオブジェクト描画
	for ( auto& obj : object3ds_ ) {
		obj->Draw();
	}

	//手札カード
	handManager_.Draw();

	levelEditor_->Draw();


	// パーティクル描画 (パイプライン切り替え)
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Particle);
	ParticleManager::GetInstance()->Draw(commandList);
	
	
	PostEffect::GetInstance()->PostDrawScene(commandList, dxCommon);
	PostEffect::GetInstance()->Draw(commandList,dxCommon);
	
	// スプライト描画の前準備
	SpriteCommon::GetInstance()->PreDraw(commandList);
	
	if (sprite_) {
		sprite_->Draw();
	}

	
}

void GamePlayScene::DrawDebugUI(){

#ifdef USE_IMGUI
	// 3Dオブジェクト、カメラ、パーティクルのUI
	Obj3dCommon::GetInstance()->DrawDebugUI();
	if ( camera_ ) { camera_->DrawDebugUI(); }
	if ( debugCamera_ ) { debugCamera_->DrawDebugUI(); }
	ParticleManager::GetInstance()->DrawDebugUI();

	
	levelEditor_->DrawDebugUI();

	// スプライト調整用UI
	ImGui::SetNextWindowSize(ImVec2(500, 100));
	ImGui::Begin("Sprite Setup");
	ImGui::DragFloat2("Position", &spritePos_.x, 0.1f, -2000.0f, 2000.0f, "% 06.1f");
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
	if (ImGui::Button("Pick Up 'Fist'(ID: 1)")) {
		handManager_.AddCard(CardDatabase::GetCardData(1));
	}
	ImGui::SameLine();
	if (ImGui::Button("Pick Up 'Fireball'(ID: 2)")) {
		handManager_.AddCard(CardDatabase::GetCardData(2));
	}

	ImGui::Separator();
	ImGui::Text("[Player Hand] : %d/10", handManager_.GetHandSize());

	//手札の数だけループしてボタンを作る
	for ( int i = 0; i < handManager_.GetHandSize(); ++i ) {
		Card card = handManager_.GetCard(i);

		//ボタンの名前
		std::string btnName = card.name + "(Cost:" + std::to_string(card.cost) + ")##" + std::to_string(i);

		// 使う処理を入れる場合はこのif文の中に書く
		if ( ImGui::Button(btnName.c_str()) ) {
			// 例：手札を使用する処理
		}
	}

	ImGui::End();

#endif

}

// デバッグ用バトルリセット
void GamePlayScene::ResetBattleDebug() {

	// プレイヤー初期化
	if (player_) {
		player_->Initialize();
		playerPos_ = { 0.0f, 0.0f, 0.0f };
		playerScale_ = { 1.0f, 1.0f, 1.0f };
		player_->SetPosition(playerPos_);
		player_->SetScale(playerScale_);
	}

	// 敵初期化
	if (enemy_) {
		enemy_->Initialize();
		enemyPos_ = { 5.0f, 0.0f, 5.0f };
		enemyScale_ = { 1.0f, 1.0f, 1.0f };
		enemy_->SetPosition(enemyPos_);
		enemy_->SetScale(enemyScale_);
	}

	// プレイヤーモデル更新
	if (playerObj_ && player_) {
		playerObj_->SetTranslation(player_->GetPosition());
		playerObj_->SetRotation(player_->GetRotation());
		playerObj_->SetScale(player_->GetScale());
		playerObj_->Update();
	}

	// 敵モデル更新
	if (enemyObj_ && enemy_) {
		enemyObj_->SetTranslation(enemy_->GetPosition());
		enemyObj_->SetRotation(enemy_->GetRotation());
		enemyObj_->SetScale(enemy_->GetScale());
		enemyObj_->Update();
	}

	// カード使用システムの状態をリセット
	if (cardUseSystem_) {
		cardUseSystem_->Reset(); // パンチ・火球などの演出状態を初期化
	}

	// 敵死亡処理フラグリセット
	enemyDeadHandled_ = false;

	// 手札を初期化
	handManager_.Initialize(camera_.get());
	handManager_.AddCard(CardDatabase::GetCardData(1)); // 初期カードを再追加

	// フィールドカードを初期化
	cardPickupManager_.Initialize(camera_.get());
	cardPickupManager_.AddPickup({ 3.0f, 0.0f, 3.0f }, CardDatabase::GetCardData(2));
	cardPickupManager_.AddPickup({ -3.0f, 0.0f, 5.0f }, CardDatabase::GetCardData(3));
}

GamePlayScene::GamePlayScene(){}

GamePlayScene::~GamePlayScene(){}
// 終了
void GamePlayScene::Finalize(){
	

	object3ds_.clear();

	textures_.clear();
	depthStencilResource_.Reset();
}