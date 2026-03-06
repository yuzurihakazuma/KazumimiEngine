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

	// ファイル名を指定するだけで、読み込み・生成・配置
	// 引数: (ファイルパス, 座標)
	sprite_ = Sprite::Create(textures_["uvChecker"].srvIndex, spritePos_);

	// スプライトの切り抜き範囲を設定 (テクスチャの左上から128x128ピクセルを使用)
	sprite_->SetTextureRect(0.0f, 0.0f, 128.0f, 128.0f, 256.0f, 256.0f);


	// デプスステンシル作成 (TextureManagerシングルトン)
	depthStencilResource_ = TextureManager::GetInstance()->CreateDepthStencilTextureResource(
		windowProc->GetClientWidth(), windowProc->GetClientHeight()
	);

	PostEffect::GetInstance()->DrawDebugUI();

	levelEditor_ = std::make_unique<LevelEditor>();
	levelEditor_->SetCamera(camera_.get());
	levelEditor_->Initialize();
}

void GamePlayScene::Update(){
	
	// デバッグカメラ更新
	if (debugCamera_) {
		debugCamera_->Update(camera_.get());
	}
	// カメラ更新
	camera_->Update();

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


	// 全オブジェクト更新
	for ( auto& obj : object3ds_ ) {
		obj->Update();
	}
	// スプライト更新
	sprite_->Update();

#ifdef USE_IMGUI

	if (input->Triggerkey(DIK_F1)) {
		isEditorActive_ = !isEditorActive_;
	}

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	// ウィンドウのタイトルバーや背景をすべて消すフラグ
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

	ImGui::Begin("MasterDockSpace", nullptr, window_flags);

	ImGui::PopStyleColor();

	ImGui::PopStyleVar(3);

	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	// ★最重要：ImGuiDockNodeFlags_PassthruCentralNode を付けることで真ん中が透明になりゲーム画面が見える！
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
	ImGui::End();


	// デバッグカメラのUI表示
	if (debugCamera_) {
		debugCamera_->DrawDebugUI();
	}

	// 3Dオブジェクト共通のUI表示
	Obj3dCommon::GetInstance()->DrawDebugUI();

	// カメラのUI表示
	if (camera_) {
		camera_->DrawDebugUI();
	}

	ParticleManager::GetInstance()->DrawDebugUI();

	PostEffect::GetInstance()->DrawDebugUI();

	// 要件2: ウィンドウサイズを(500, 100)に固定
	ImGui::SetNextWindowSize(ImVec2(500, 100));

	// 要件1: 専用のImGuiウィンドウを作成
	ImGui::Begin("Sprite Setup");

	// 要件4: 小数第1位まで表示するため、最後の引数に "%.1f" を指定する！
	ImGui::DragFloat2("Position", &spritePos_.x, 0.1f, -2000.0f, 2000.0f,"% 06.1f");

	ImGui::End();

	//------------カードシステム単体テスト------------
	ImGui::Begin("Card System Test");

	// 仮のプレイヤーコスト状況を表示
	ImGui::Text("Player Cost: %d", dummyPlayerCost_);
	if (ImGui::Button("Turn End (Reset Cost)")) {
		dummyPlayerCost_ = 3; //コスト回復
	}

	ImGui::Separator();

	//ダンジョンでカードを拾う
	ImGui::Text("[Dungeon Floor]");
	if (ImGui::Button("Pick Up 'Sword'(Cost: 1)")) {
		handManager_.AddCard({ 1,"Sword",1 });
	}

	ImGui::SameLine();

	if (ImGui::Button("Pick Up 'Fireball' (Cost: 2)")) {
		handManager_.AddCard({ 2,"Fireball",2 });
	}

	ImGui::Separator();

	//今の手札を表示して使う
	ImGui::Text("[Player Hand] : %d/10", handManager_.GetHandSize());

	//手札の数だけループしてボタンを作る
	for (int i = 0; i < handManager_.GetHandSize(); ++i) {
		Card card = handManager_.GetCard(i);

		//ボタンの名前
		std::string btnName = card.name + "(Cost:" + std::to_string(card.cost) + ")##" + std::to_string(i);

		if (ImGui::Button(btnName.c_str())) {
			//コスト足りるかチェック
			if (dummyPlayerCost_ >= card.cost) {
				dummyPlayerCost_ -= card.cost; //コスト消費
				handManager_.RemoveCard(i);    //手札から消す

				ImGui::LogText("Used %s!\n", card.name.c_str());
			} else {
				ImGui::LogText("Not enough cost for %s!\n", card.name.c_str());
			}
		}
	}


	ImGui::End();
	

	if (sprite_) {
		sprite_->SetPosition(spritePos_);
		sprite_->Update();
	}

	// カメラ更新
	camera_->Update();



#endif

	levelEditor_->Update(isEditorActive_);
}

void GamePlayScene::Draw(){


	auto dxCommon = DirectXCommon::GetInstance();
	auto commandList = DirectXCommon::GetInstance()->GetCommandList();
	
	// 画用紙への切り替え
	PostEffect::GetInstance()->PreDrawScene(commandList, dxCommon);


	// 3D描画の前準備
	Obj3dCommon::GetInstance()->PreDraw(commandList);

	
	
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Object3D_CullNone);
	
	
	// 3Dオブジェクト描画
	for ( auto& obj : object3ds_ ) {
		obj->Draw();
	}

	levelEditor_->Draw();


	// パーティクル描画 (パイプライン切り替え)
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Particle);
	ParticleManager::GetInstance()->Draw(commandList);
	
	
	PostEffect::GetInstance()->PostDrawScene(commandList, dxCommon);
	PostEffect::GetInstance()->Draw(commandList);
	
	// スプライト描画の前準備
	SpriteCommon::GetInstance()->PreDraw(commandList);
	
	if (sprite_) {
		sprite_->Draw();
	}

	
}

GamePlayScene::GamePlayScene(){}

GamePlayScene::~GamePlayScene(){}
// 終了
void GamePlayScene::Finalize(){
	

	object3ds_.clear();

	textures_.clear();
	depthStencilResource_.Reset();
}