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
	textures_["noise0"] = { TextureManager::GetInstance()->LoadTextureAndCreateSRV("Resources/noise0.png", commandList) };
	textures_["noise1"] = { TextureManager::GetInstance()->LoadTextureAndCreateSRV("Resources/noise1.png", commandList) };
	
	// カメラ生成
	camera_ = std::make_unique<Camera>(windowProc->GetClientWidth(), windowProc->GetClientHeight(), dxCommon);
	camera_->SetTranslation({ 0.0f, 2.0f, -15.0f });
	
	// デバッグカメラ生成
	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize();

	// プレイヤーオブジェクト生成
	testObj_ = Obj3d::Create("block");
	if ( testObj_ ){

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
		//AudioManager::GetInstance()->PlayWave(bgmFile_);
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
	if ( testObj_ ){
		testObj_->Update();
	}

	PostEffect::GetInstance()->Update();

	levelEditor_->Update();
}

void GamePlayScene::Draw(){


	auto dxCommon = DirectXCommon::GetInstance();
	auto commandList = DirectXCommon::GetInstance()->GetCommandList();
	
	// スプライト描画の前準備
	SpriteCommon::GetInstance()->PreDraw(commandList);

//	sprite_->Draw();

	// 画用紙への切り替え
	PostEffect::GetInstance()->PreDrawScene(commandList, dxCommon);


	// 3D描画の前準備
	Obj3dCommon::GetInstance()->PreDraw(commandList);

	
	if (playerObj_) {
		playerObj_->Draw();
	}
	
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Object3D_CullNone);
	
	if ( testObj_ ){
		testObj_->Draw();
	}
	
	// 3Dオブジェクト描画
	for ( auto& obj : object3ds_ ) {
		obj->Draw();
	}

	levelEditor_->Draw();


	// パーティクル描画 (パイプライン切り替え)
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Particle);
	ParticleManager::GetInstance()->Draw(commandList);
	
	
	PostEffect::GetInstance()->PostDrawScene(commandList, dxCommon);
	PostEffect::GetInstance()->Draw(commandList,dxCommon);
	
	

	
}

void GamePlayScene::DrawDebugUI(){

#ifdef USE_IMGUI
	// 3Dオブジェクト、カメラ、パーティクルのUI
	Obj3dCommon::GetInstance()->DrawDebugUI();
	if ( camera_ ) { camera_->DrawDebugUI(); }
	if ( debugCamera_ ) { debugCamera_->DrawDebugUI(); }
	ParticleManager::GetInstance()->DrawDebugUI();

	
	levelEditor_->DrawDebugUI();

	// ==========================================
	// ★追加：文字入力テスト用のウィンドウ
	ImGui::Begin("テキスト表示テスト (Text Test)");

	// 1. 文字の入力欄 (最大256文字)
	ImGui::InputText("表示する文字", inputTextBuffer_, sizeof(inputTextBuffer_));

	// 2. 決定ボタン
	if ( ImGui::Button("文字を確定して表示！") ) {
		// 入力された文字(char配列)を、string型に変換して保存する
		displayText_ = inputTextBuffer_;
	}

	ImGui::Separator(); // 区切り線

	// 3. 描画位置の調整スライダー
	ImGui::DragFloat("X座標", &textPosX_, 1.0f);
	ImGui::DragFloat("Y座標", &textPosY_, 1.0f);

	ImGui::End();

	ImGui::Begin("Block Dissolve Test");

	// スライダーで 0.0(通常) 〜 1.0(消滅) を操作
	if ( ImGui::SliderFloat("ブロックの消滅度", &dissolveThreshold_, 0.0f, 1.0f) ) {
		if ( testObj_ ) {
			// スライダーを動かすと、このブロックの閾値だけが書き換わる
			testObj_->SetDissolveThreshold(dissolveThreshold_);
		}
	}

	// 便利なリセットボタン
	if ( ImGui::Button("元に戻す") ) {
		dissolveThreshold_ = 0.0f;
		if ( testObj_ ){

			testObj_->SetDissolveThreshold(0.0f);
		}
			
	}
	ImGui::SameLine();
	if ( ImGui::Button("完全に消す") ) {
		dissolveThreshold_ = 1.0f;
		if ( testObj_ ){
			testObj_->SetDissolveThreshold(1.0f);
		}
	}

	ImGui::End();
	if ( !displayText_.empty() ) {
		// 背景用のキャンバスを取得
		ImDrawList* drawList = ImGui::GetBackgroundDrawList();

		// textPosX_, textPosY_ の位置に、白色で displayText_ を描画する！
		// ※変数(std::string)の中身を取り出すには .c_str() を使います
		drawList->AddText(
			ImVec2(textPosX_, textPosY_),
			IM_COL32(255, 255, 255, 255),
			displayText_.c_str()
		);
	}

#endif

}

GamePlayScene::GamePlayScene(){}

GamePlayScene::~GamePlayScene(){}
// 終了
void GamePlayScene::Finalize(){
	

	object3ds_.clear();

	textures_.clear();
	depthStencilResource_.Reset();
}