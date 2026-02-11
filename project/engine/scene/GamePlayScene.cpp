#include "GamePlayScene.h"
#include "Matrix4x4.h"
#include "ImGuiManager.h"

// シングルトンクラスのヘッダー
#include "AudioManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "TextureManager.h"
#include "PipelineManager.h"
#include "SceneManager.h"
#include "TitleScene.h"
#include "Camera.h"
#include "Sprite.h"
#include "Obj3d.h"
#include "Input.h"
#include "SpriteCommon.h"
#include "Obj3dCommon.h"

using namespace MatrixMath;
// 初期化
void GamePlayScene::Initialize(){
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	WindowProc* windowProc = WindowProc::GetInstance();

	
	// コマンドリストの記録開始 (リソース作成に必要)
	dxCommon->BeginCommandRecording();
	
	// コマンドリスト取得
	auto commandList = dxCommon->GetCommandList();

	// BGMロード (シングルトン)
	AudioManager::GetInstance()->LoadWave(bgmFile_);
	// モデル読み込み (シングルトン)
	ModelManager::GetInstance()->LoadModel("fence", "resources", "fence.obj");
	// 球モデル作成 (シングルトン)
	ModelManager::GetInstance()->CreateSphereModel("sphere", 16);
	// パーティクルグループ作成 (シングルトン)
	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "resources/uvChecker.png");

	// テクスチャ読み込み
	textureResource_ = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/uvChecker.png", commandList);
	textureResource2_ = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/monsterBall.png", commandList);
	textureResource3_ = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/fence.png", commandList);
	textureResource5_ = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/circle.png", commandList);

	// コマンドリストの終了
	dxCommon->EndCommandRecording();


	// カメラ生成
	camera_ = std::make_unique<Camera>(windowProc->GetClientWidth(), windowProc->GetClientHeight(), dxCommon);
	camera_->SetTranslation({ 0.0f, 0.0f, -10.0f });

	// オブジェクト生成
	fence_ = std::make_unique<Obj3d>();
	sphere_ = std::make_unique<Obj3d>();

	// モデル取得 (シングルトンから)
	Model* modelGround = ModelManager::GetInstance()->FindModel("fence");
	Model* modelSphere = ModelManager::GetInstance()->FindModel("sphere");

	// オブジェクト初期化
	fence_->Initialize(modelGround); 
	fence_->SetCamera(camera_.get());
	
	// 床の位置とスケール設定
	sphere_->Initialize(modelSphere);
	sphere_->SetCamera(camera_.get());
	sphere_->SetTranslation(spherePos_);
	sphere_->SetScale(sphereScale_);

	// スプライト生成
	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize();
	// ハンドル取得には TextureResource 内の srvIndex を使う
	sprite_->SetTextureHandle(dxCommon->GetSrvManager()->GetGPUDescriptorHandle(textureResource_.srvIndex));

	// デプスステンシル作成 (TextureManagerシングルトン)
	depthStencilResource_ = TextureManager::GetInstance()->CreateDepthStencilTextureResource(
		windowProc->GetClientWidth(), windowProc->GetClientHeight()
	);
}

void GamePlayScene::Update(){
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

	// オブジェクト更新
	if ( sphere_ ) {
		sphere_->SetTranslation(spherePos_);
		sphere_->SetScale(sphereScale_);
		sphere_->Update();
	}
	// 全オブジェクト更新
	for ( auto& obj : object3ds_ ) {
		obj->Update();
	}
	// スプライト更新
	sprite_->Update();

#ifdef USE_IMGUI

	
	ImGui::Text("Current Scene: GamePlay");
	ImGui::Separator(); // 区切り線


	ImGui::Begin("Debug");
	ImGui::DragFloat3("Pos", &groundPos_.x, 0.1f);
	ImGui::DragFloat3("Scale", &groundScale_.x, 0.1f);
	ImGui::DragFloat3("SpherePos", &spherePos_.x, 0.1f);
	ImGui::DragFloat3("SphereScale", &sphereScale_.x, 0.1f);

	if ( ImGui::TreeNode("Directional Light") ) {
		auto light = Obj3dCommon::GetInstance()->GetLightData();
		ImGui::DragFloat3("Direction", &light->direction.x, 0.01f);
		ImGui::ColorEdit3("Color", &light->color.x);
		ImGui::DragFloat("Intensity", &light->intensity, 0.01f, 0.0f, 10.0f);
		light->direction = Normalize(light->direction);
		ImGui::TreePop();
	}

	if ( ImGui::TreeNode("Camera") ) {
		Vector3 camPos = camera_->GetTransform().translate;
		Vector3 camRot = camera_->GetTransform().rotate;
		ImGui::DragFloat3("Position", &camPos.x, 0.1f);
		ImGui::DragFloat3("Rotation", &camRot.x, 0.01f);
		camera_->SetTranslation(camPos);
		camera_->SetRotation(camRot);
		ImGui::TreePop();
	}

	ImGui::Text("=== Particle Debug ===");
	size_t particleCount = ParticleManager::GetInstance()->GetParticleCount("Circle");
	uint32_t instanceCount = ParticleManager::GetInstance()->GetInstanceCount("Circle");
	ImGui::Text("Particles (CPU) : %zu", particleCount);
	ImGui::Text("Instances (GPU) : %u", instanceCount);

	if ( ImGui::Button("Emit 10") ) {
		ParticleManager::GetInstance()->Emit("Circle", { 0.0f, 5.0f, 0.0f }, 10);
	}
	ImGui::End();

	if ( !object3ds_.empty() ) {
		object3ds_[0]->SetTranslation(groundPos_);
	}
#endif


}

void GamePlayScene::Draw(){

	auto commandList = DirectXCommon::GetInstance()->GetCommandList();
	
	// スプライト・3D描画の前準備
	SpriteCommon::GetInstance()->PreDraw(commandList);

	// 3D描画の前準備
	Obj3dCommon::GetInstance()->PreDraw(commandList);

	if (fence_) {
		fence_->Draw();
	}
	if (sphere_) {
		sphere_->Draw();
	}

	// 3Dオブジェクト描画
	for ( auto& obj : object3ds_ ) {
		obj->Draw();
	}

	// パーティクル描画 (パイプライン切り替え)
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Particle);
	ParticleManager::GetInstance()->Draw(commandList);
	
	
	// 床描画
	sprite_->Draw();
}

GamePlayScene::GamePlayScene(){}

GamePlayScene::~GamePlayScene(){}

void GamePlayScene::Finalize(){
	

	object3ds_.clear();

	textureResource_.resource.Reset();
	depthStencilResource_.Reset();
}