#include "TitleScene.h"

// ゲーム固有のファイル
#include "GamePlayScene.h"

// エンジン側のファイル
#include "Engine/Utils/ImGuiManager.h"
#include "Engine/Utils/Color.h"
#include "Engine/Utils/TextManager.h"
#include "Engine/Audio/AudioManager.h"
#include "Engine/3D/Model/ModelManager.h"
#include "Engine/Particle/ParticleManager.h"
#include "Engine/Graphics/TextureManager.h"
#include "Engine/Graphics/PipelineManager.h"
#include "Engine/Scene/SceneManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Camera/DebugCamera.h"
#include "Engine/3D/Obj/Obj3d.h"
#include "Engine/Base/Input.h"
#include "Engine/3D/Obj/Obj3dCommon.h"
#include "Engine/Base/DirectXCommon.h"
#include "Engine/Base/WindowProc.h"
#include "engine/math/VectorMath.h"
#include "engine/postEffect/PostEffect.h"
#include "game/map/MapManager.h"
#include"engine/utils/Level/LevelEditor.h"
#include "engine/utils/EditorManager.h"
#include <cstdlib>

#include "engine/graphics/SrvManager.h"
#include "Bloom.h"

using namespace VectorMath;
using namespace MatrixMath;

TitleScene::TitleScene() {}

TitleScene::~TitleScene() {}

void TitleScene::Initialize() {
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	WindowProc* windowProc = WindowProc::GetInstance();

	// コマンドリスト取得
	auto commandList = dxCommon->GetCommandList();

	// BGMロード
	AudioManager::GetInstance()->LoadWave(bgmFile_);

	// モデル読み込み
	ModelManager::GetInstance()->LoadModel("fence", "resources", "fence.obj");
	ModelManager::GetInstance()->LoadModel("grass", "resources", "terrain.obj");
	ModelManager::GetInstance()->LoadModel("block", "resources/block", "block.obj");
	ModelManager::GetInstance()->LoadModel("wall", "resources/wall", "wall.obj");
	ModelManager::GetInstance()->LoadModel("stairs", "resources/stairs", "stairs.obj");
	ModelManager::GetInstance()->LoadModel("ground", "resources/Ground", "Ground.obj");

	// 球モデル作成
	ModelManager::GetInstance()->CreateSphereModel("sphere", 16);

	// パーティクルグループ作成
	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "resources/uvChecker.png");

	// テクスチャ読み込み
	textures_["uvChecker"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/uvChecker.png", commandList);
	textures_["monsterBall"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/monsterBall.png", commandList);
	textures_["fence"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/fence.png", commandList);
	textures_["circle"] = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/circle.png", commandList);

	// カメラ生成
	camera_ = std::make_unique<Camera>(
		windowProc->GetClientWidth(),
		windowProc->GetClientHeight(),
		dxCommon
	);
	camera_->SetTranslation({ 0.0f, 2.0f, -15.0f });

	// デバッグカメラ生成
	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize();

	// デプスステンシル作成
	depthStencilResource_ = TextureManager::GetInstance()->CreateDepthStencilTextureResource(
		windowProc->GetClientWidth(),
		windowProc->GetClientHeight()
	);

	// 必要ならマップエディタ初期化
	mapManager_ = std::make_unique<MapManager>();
	mapManager_->SetCamera(camera_.get());
	mapManager_->Initialize();

	// タイトル背景用にマップ中央を真上から見る
	const LevelData& level = mapManager_->GetLevelData();

	titleBgCameraPos_ = {
		((float)level.width - 1.0f) * level.tileSize * 0.5f,
		35.0f,
		((float)level.height - 1.0f) * level.tileSize * 0.5f
	};

	titleBgPlayerPos_ = mapManager_->GetMapCenterPosition();

	camera_->SetTranslation(titleBgCameraPos_);
	camera_->SetRotation(titleBgCameraRot_);
	camera_->SetFovY(1.0f);
	camera_->SetFarClip(200.0f);
	camera_->Update();

	titleBgCameraPos_ = { 49.0f, 7.4f, 49.1f };
	titleBgCameraRot_ = { 1.57f, -1.57f, 0.0f };

	// 全部壁ブロックで敷き詰めたい場合だけ使う
	mapManager_->FillAllTiles(1);

	// タイトル用テキストを設定
	TextManager::GetInstance()->Initialize();
	const float screenW = static_cast<float>(windowProc->GetClientWidth());
	const float screenH = static_cast<float>(windowProc->GetClientHeight());
	// タイトルは複数行なので、画面中央付近に相対オフセットで置いて確実に見えるようにする
	TextManager::GetInstance()->SetPosition("SceneMessage", screenW * 0.5f - 220.0f, screenH * 0.5f - 100.0f);
	TextManager::GetInstance()->SetCentered("SceneMessage", false);
	TextManager::GetInstance()->SetText(
		"SceneMessage",
		"TITLE\n\nPRESS SPACE TO START\nPRESS 0 FOR TUTORIAL"
	);

	// モデル読み込み（カード）
	ModelManager::GetInstance()->LoadModel("cardR", "resources/card", "CardR.obj");
	ModelManager::GetInstance()->LoadModel("cardF", "resources/card", "cardF.obj");
	ModelManager::GetInstance()->LoadModel("cardFire", "resources/card", "CardFire.obj");
	ModelManager::GetInstance()->LoadModel("cardPotion", "resources/card", "CardPotion.obj");
	ModelManager::GetInstance()->LoadModel("cardSpeedUp", "resources/card", "CardSpeedUp.obj");
	ModelManager::GetInstance()->LoadModel("CardShield", "resources/card", "CardShield.obj");
	ModelManager::GetInstance()->LoadModel("CardIce", "resources/card", "CardIce.obj");
	ModelManager::GetInstance()->LoadModel("CardFang", "resources/card", "CardFang.obj");
	ModelManager::GetInstance()->LoadModel("CardDecoy", "resources/card", "CardDecoy.obj");
	ModelManager::GetInstance()->LoadModel("CardAtkDown", "resources/card", "CardAtkDown.obj");
	ModelManager::GetInstance()->LoadModel("CardClaw", "resources/card", "CardClaw.obj");
	ModelManager::GetInstance()->LoadModel("CardScanner", "resources/card", "MapOpen.obj");

	// CSVからカードデータベースを初期化
	InitializeTitleCardRain();

}

void TitleScene::Update() {
	// デバッグカメラ更新
	if (debugCamera_) {
		debugCamera_->Update(camera_.get());
	}

	titleBgCameraPos_ = { 49.0f, 16.0f, 49.1f };
	titleBgCameraRot_ = { 1.57f, -1.57f, 0.0f };

	camera_->SetTranslation(titleBgCameraPos_);
	camera_->SetRotation(titleBgCameraRot_);
	camera_->SetFovY(1.0f);
	camera_->SetFarClip(200.0f);
	camera_->Update();



	auto* light = Obj3dCommon::GetInstance()->GetLightData();
	if (light) {
		light->direction = Normalize({ 0.038f, -0.997f, -0.074f });
		light->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		light->intensity = 0.43f;
	}


	// マップエディタ更新
	if (mapManager_) {
		mapManager_->Update(titleBgPlayerPos_);
	}

	// タイトル背景のカード雨更新
	UpdateTitleCardRain();

	Input* input = Input::GetInstance();

	// BGM再生はBキーに分ける
	if (input->Triggerkey(DIK_B)) {
		AudioManager::GetInstance()->PlayWave(bgmFile_);
	}

	// SPACEでゲーム開始
	if (input->Triggerkey(DIK_SPACE)) {
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
		return;
	}

	if (input->Triggerkey(DIK_0)) {
		GamePlayScene::RequestTutorialStart(true);
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
		return;
	}

	// パーティクル発生
	if (input->Triggerkey(DIK_P)) {
		ParticleManager::GetInstance()->Emit("Circle", { 0.0f, 0.0f, 0.0f }, 10);
	}

	// パーティクル更新
	ParticleManager::GetInstance()->Update(camera_.get());

	// 3Dオブジェクト更新
	for (auto& obj : object3ds_) {
		if (obj) {
			obj->Update();
		}
	}
}

void TitleScene::DrawDebugUI() {
#ifdef USE_IMGUI
	Obj3dCommon::GetInstance()->DrawDebugUI();

	if (camera_) {
		camera_->DrawDebugUI();
	}
	if (debugCamera_) {
		debugCamera_->DrawDebugUI();
	}

	ParticleManager::GetInstance()->DrawDebugUI();

	if (mapManager_) {
		mapManager_->DrawDebugUI();
	}



	ImGui::SetNextWindowSize(ImVec2(500, 100), ImGuiCond_FirstUseEver);
	ImGui::Begin("TitleScene Debug");
	ImGui::Text("Enter : Start Game");
	ImGui::Text("B : Play BGM");
	ImGui::End();
#endif
}

void TitleScene::Draw(){
	auto dxCommon = DirectXCommon::GetInstance();
	auto commandList = dxCommon->GetCommandList();

	// 1. MRT開始（色とマスク用キャンバス）
	PostEffect::GetInstance()->PreDrawSceneMRT(commandList);

	// --- ここから3Dやパーティクルの描画 ---
	Obj3dCommon::GetInstance()->PreDraw(commandList);
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Object3D_CullNone);

	for ( auto& obj : object3ds_ ) {
		if ( obj ) {
			obj->Draw();
		}
	}

	if (mapManager_) {
		mapManager_->Draw(titleBgPlayerPos_);
	}

	Obj3dCommon::GetInstance()->PreDraw(commandList);
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Object3D_CullNone);

	for (auto& card : titleRainCards_) {
		if (card.obj) {
			card.obj->Draw();
		}
	}
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Particle);
	ParticleManager::GetInstance()->Draw(commandList);
	// --- 描画ここまで ---

	// 2. MRT終了
	PostEffect::GetInstance()->PostDrawSceneMRT(commandList);

	// 3. ポストエフェクト適用
	PostEffect::GetInstance()->Draw(commandList);

	// 4. Bloomパス
	uint32_t colorSrv = PostEffect::GetInstance()->GetSrvIndex();
	uint32_t maskSrv = PostEffect::GetInstance()->GetMaskSrvIndex();
	Bloom::GetInstance()->Render(commandList, colorSrv, maskSrv);
	uint32_t finalSrv = Bloom::GetInstance()->GetResultSrvIndex();

	// 5. バックバッファへ最終出力
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dxCommon->GetBackBufferRtvHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon->GetDsvHandle();
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
	PipelineManager::GetInstance()->SetPostEffectPipeline(commandList, PostEffectType::None);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	SrvManager::GetInstance()->SetGraphicsRootDescriptorTable(0, finalSrv);
	commandList->DrawInstanced(3, 1, 0, 0);

	// 6. UI（テキスト）描画
	TextManager::GetInstance()->Draw();

	// エディタ用の出力設定
	EditorManager::GetInstance()->SetGameViewSrvIndex(finalSrv);
}

void TitleScene::Finalize() {
	object3ds_.clear();
	mapManager_.reset();

	// タイトル用テキストを消す
	TextManager::GetInstance()->SetText("SceneMessage", "");
	TextManager::GetInstance()->Finalize();

	textures_.clear();
	depthStencilResource_.Reset();
}

// minValue以上maxValue未満の範囲でランダムなfloatを生成する関数
float RandomRange(float minValue, float maxValue) {
	float t = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	return minValue + (maxValue - minValue) * t;
}

// タイトル背景のカード雨の初期化
void TitleScene::InitializeTitleCardRain() {
	titleRainCards_.clear();
	titleRainCards_.resize(titleRainCardCount_);

	for (auto& card : titleRainCards_) {
		ResetTitleRainCard(card, true);
	}
}

// タイトル背景のカード雨のリセット（位置や回転をランダムに再設定）
void TitleScene::ResetTitleRainCard(TitleRainCard& card, bool randomY) {
	if (titleRainCardModelNames_.empty()) {
		return;
	}

	int modelIndex = rand() % static_cast<int>(titleRainCardModelNames_.size());
	const std::string& modelName = titleRainCardModelNames_[modelIndex];

	card.obj = Obj3d::Create(modelName);

	card.position = {
	randomY ? RandomRange(titleRainSpawnX_, titleRainResetX_) : titleRainSpawnX_,
	titleRainY_,
	RandomRange(titleRainSpawnMinZ_, titleRainSpawnMaxZ_)
	};

	card.rotation = {
		RandomRange(0.0f, 6.28f),
		RandomRange(0.0f, 6.28f),
		RandomRange(0.0f, 6.28f)
	};

	card.rotationSpeed = {
		RandomRange(-0.03f, 0.03f),
		RandomRange(-0.05f, 0.05f),
		RandomRange(-0.04f, 0.04f)
	};

	card.fallSpeed = RandomRange(0.04f, 0.12f);

	if (card.obj) {
		card.obj->SetCamera(camera_.get());
		card.obj->SetTranslation(card.position);
		card.obj->SetRotation(card.rotation);
		card.obj->SetScale({ titleRainScale_, titleRainScale_, titleRainScale_ });
		card.obj->Update();
	}
}

// タイトル背景のカード雨の更新（位置を落下させ、回転させる）
void TitleScene::UpdateTitleCardRain() {
	for (auto& card : titleRainCards_) {
		if (!card.obj) {
			ResetTitleRainCard(card, false);
			continue;
		}

		card.position.x += card.fallSpeed;

		card.rotation.x += card.rotationSpeed.x;
		card.rotation.y += card.rotationSpeed.y;
		card.rotation.z += card.rotationSpeed.z;

		if (card.position.x > titleRainResetX_) {
			ResetTitleRainCard(card, false);
			continue;
		}

		card.obj->SetTranslation(card.position);
		card.obj->SetRotation(card.rotation);
		card.obj->SetScale({ titleRainScale_, titleRainScale_, titleRainScale_ });
		card.obj->Update();
	}
}
