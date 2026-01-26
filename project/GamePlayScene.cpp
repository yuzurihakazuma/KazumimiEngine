#include "GamePlayScene.h"
#include "Matrix4x4.h"
#include "ImGuiManager.h"

// シングルトンクラスのヘッダー
#include "AudioManager.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "TextureManager.h"
#include "PipelineManager.h"

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
	camera_ = new Camera(windowProc->GetClientWidth(), windowProc->GetClientHeight(), dxCommon);
	camera_->SetTranslation({ 0.0f, 0.0f, -10.0f });

	// オブジェクト生成
	fence_ = new Obj3d();
	sphere_ = new Obj3d();

	// モデル取得 (シングルトンから)
	Model* modelGround = ModelManager::GetInstance()->FindModel("fence");
	Model* modelSphere = ModelManager::GetInstance()->FindModel("sphere");

	// オブジェクト初期化
	fence_->Initialize(modelGround); 
	fence_->SetCamera(camera_);
	
	// 床の位置とスケール設定
	sphere_->Initialize(modelSphere);
	sphere_->SetCamera(camera_);
	sphere_->SetTranslation(spherePos_);
	sphere_->SetScale(sphereScale_);
	// 小さくて見えない対策（任意）
	object3ds_.push_back(sphere_);

	// スプライト生成
	sprite_ = new Sprite();
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

	// パーティクル発生 (シングルトン)
	if ( input->Triggerkey(DIK_P) ) {
		ParticleManager::GetInstance()->Emit("Circle", { 0.0f, 0.0f, 0.0f }, 10);
	}
	// パーティクル更新
	ParticleManager::GetInstance()->Update(camera_);

	// オブジェクト更新
	if ( sphere_ ) {
		sphere_->SetTranslation(spherePos_);
		sphere_->SetScale(sphereScale_);
	}
	// 全オブジェクト更新
	for ( Obj3d* obj : object3ds_ ) {
		obj->Update();
	}
	// スプライト更新
	sprite_->Update();


#ifdef USE_IMGUI
	
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
	Obj3dCommon::GetInstance()->PreDraw(commandList);

	// 3Dオブジェクト描画
	for ( Obj3d* obj : object3ds_ ) {
		obj->Draw();
	}

	// パーティクル描画 (パイプライン切り替え)
	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Particle);
	ParticleManager::GetInstance()->Draw(commandList);

	// スプライト描画などがあればここに追加
	// sprite_->Draw(); 
}

void GamePlayScene::Finalize(){
	// ゲーム固有の解放
	delete camera_;
	delete sprite_;
	delete fence_;
	delete sphere_;
	// vectorはクリアされるが中身のポインタ削除
	// (object3ds_に入っているポインタと重複しないよう管理が必要ですが、一旦シンプルに)
	object3ds_.clear();

	textureResource_.resource.Reset();
	depthStencilResource_.Reset();
}