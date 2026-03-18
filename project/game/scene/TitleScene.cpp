#include "TitleScene.h"

// --- ゲーム固有のファイル ---
#include "GamePlayScene.h"

// --- エンジン側のファイル ---
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
#include "Engine/3D/Obj/Obj3d.h"
#include "Engine/Base/Input.h"
#include "Engine/2D/SpriteCommon.h"
#include "Engine/3D/Obj/Obj3dCommon.h"
#include "Engine/Base/DirectXCommon.h"
#include "Engine/Base/WindowProc.h"
#include "engine/math/VectorMath.h"
#include "engine/graphics/RenderTexture.h"
#include "engine/graphics/SrvManager.h"
#include "engine/postEffect/PostEffect.h"
#include "engine/utils/Level/LevelEditor.h"

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

	// スプライト生成
	sprite_ = Sprite::Create(textures_["uvChecker"].srvIndex, spritePos_);

	// デプスステンシル作成
	depthStencilResource_ = TextureManager::GetInstance()->CreateDepthStencilTextureResource(
		windowProc->GetClientWidth(),
		windowProc->GetClientHeight()
	);

	// 必要ならマップエディタ初期化
	levelEditor_ = std::make_unique<LevelEditor>();
	levelEditor_->SetCamera(camera_.get());
	levelEditor_->Initialize();
}

void TitleScene::Update() {
	// デバッグカメラ更新
	if (debugCamera_) {
		debugCamera_->Update(camera_.get());
	}

	// カメラ更新
	if (camera_) {
		camera_->Update();
	}

	Input* input = Input::GetInstance();

	// BGM再生
	if (input->Triggerkey(DIK_SPACE)) {
		AudioManager::GetInstance()->PlayWave(bgmFile_);
	}

	// ゲーム開始
	if (input->Triggerkey(DIK_RETURN) || input->Triggerkey(DIK_SPACE)) {
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

	// スプライト更新
	if (sprite_) {
		sprite_->SetPosition(spritePos_);
		sprite_->Update();
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

	if (levelEditor_) {
		levelEditor_->DrawDebugUI();
	}

	ImGui::SetNextWindowSize(ImVec2(500, 100), ImGuiCond_FirstUseEver);
	ImGui::Begin("TitleScene Debug");
	ImGui::DragFloat2("Sprite Position", &spritePos_.x, 0.1f, -2000.0f, 2000.0f, "%.1f");
	ImGui::Text("Enter or T : Start Game");
	ImGui::Text("Space : Play BGM");
	ImGui::End();
#endif
}

void TitleScene::Draw() {
	auto dxCommon = DirectXCommon::GetInstance();
	auto commandList = dxCommon->GetCommandList();

	// 画用紙への切り替え
	PostEffect::GetInstance()->PreDrawScene(commandList, dxCommon);

	// 3D描画前準備
	Obj3dCommon::GetInstance()->PreDraw(commandList);
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Object3D_CullNone);

	// 3Dオブジェクト描画
	for (auto& obj : object3ds_) {
		if (obj) {
			obj->Draw();
		}
	}

	// パーティクル描画
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Particle);
	ParticleManager::GetInstance()->Draw(commandList);

	// スプライト描画
	SpriteCommon::GetInstance()->PreDraw(commandList);
	if (sprite_) {
		sprite_->Draw();
	}

	PostEffect::GetInstance()->PostDrawScene(commandList, dxCommon);
	PostEffect::GetInstance()->Draw(commandList, dxCommon);
}

void TitleScene::Finalize() {
	object3ds_.clear();
	sprites_.clear();
	sprite_.reset();
	levelEditor_.reset();

	textures_.clear();
	depthStencilResource_.Reset();
}