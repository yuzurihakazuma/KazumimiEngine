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

}

void GamePlayScene::Update(){
	
	// デバッグカメラ更新
	if (debugCamera_) {
		debugCamera_->Update(camera_.get());
	}

	Input* input = Input::GetInstance();

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
		player_->Update();
		playerPos_ = player_->GetPosition();
		playerScale_ = player_->GetScale();
	}

	if (playerObj_) {
		playerObj_->SetTranslation(playerPos_);
		playerObj_->SetRotation(player_->GetRotation());
		playerObj_->SetScale(playerScale_);
		playerObj_->Update();
	}

	for (auto& pickup : cardPickupManager_.GetPickups()) {
		if (!pickup.isActive) {
			continue;
		}

		Vector3 diff = {
			playerPos_.x - pickup.position.x,
			playerPos_.y - pickup.position.y,
			playerPos_.z - pickup.position.z
		};

		float dist = Length(diff);

		if (dist < 2.0f) {
			bool success = handManager_.AddCard(pickup.card);
			if (success) {
				pickup.isActive = false;
			}
		}
	}

	cardPickupManager_.Update();

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
	handManager_.Update();

	//SPACEキーを押した瞬間
	if (input->Triggerkey(DIK_SPACE)) {

		//今選んでいるカードのデータを取得
		Card selectedCard = handManager_.GetSelectedCard();

		//カードがない状態じゃなければ使用処理を行う
		if (selectedCard.id != -1) {

			//プレイヤーのコストが足りているかチェック
			if (dummyPlayerCost_ >= selectedCard.cost) {

				//コストを減らす
				dummyPlayerCost_ -= selectedCard.cost;

				//ここでカードの効果を発動する処理を書く


				//IDが1じゃないときだけ手札から消す
				if (selectedCard.id != 1) {
					//使ったカードを手札から消す
					handManager_.RemoveSelectedCard();
				}
			} else {
				//コスト不足の時の処理
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

	
	if (playerObj_) {
		playerObj_->Draw();
	}
	
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Object3D_CullNone);
	
	if (playerObj_) {
		playerObj_->Draw();
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

	// 仮のプレイヤーコスト状況を表示
	ImGui::Text("Player Cost: %d", dummyPlayerCost_);
	if ( ImGui::Button("Turn End (Reset Cost)") ) {
		dummyPlayerCost_ = 3; //コスト回復
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

	levelEditor_->Update();
}



GamePlayScene::GamePlayScene(){}

GamePlayScene::~GamePlayScene(){}
// 終了
void GamePlayScene::Finalize(){
	

	object3ds_.clear();

	textures_.clear();
	depthStencilResource_.Reset();
}