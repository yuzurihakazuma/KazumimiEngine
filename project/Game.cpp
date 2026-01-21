#include "Game.h"

void Game::Initialize(){

	Framework::Initialize();

	// ---------------------------------------------
	// ここから先は「このゲーム固有」の初期化
	// ---------------------------------------------

	dxCommon_->BeginCommandRecording();

	// BGMロード
	AudioManager::GetInstance()->LoadWave(bgmFile_);

	// モデル・テクスチャロード
	ModelManager::GetInstance()->LoadModel("Ground", "resources", "fence.obj");
	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "resources/circle.png");

	auto commandList = dxCommon_->GetCommandList();
	textureResource_ = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/uvChecker.png", commandList);

	// 2枚目：monsterBall

	textureResource2_ = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/monsterBall.png", commandList);

	// 3枚目：fence

	textureResource3_ = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/fence.png", commandList);

	// 4枚目：circle

	textureResource5_ = TextureManager::GetInstance()->LoadTextureAndCreateSRV("resources/circle.png", commandList);




	dxCommon_->EndCommandRecording();

	// カメラ生成
	camera_ = new Camera(windowProc_->GetClientWidth(), windowProc_->GetClientHeight());
	camera_->SetTranslation({ 0.0f, 0.0f, -10.0f });

	// オブジェクト生成
	Obj3d* groundObj = new Obj3d();
	Model* modelGround = ModelManager::GetInstance()->FindModel("Ground");
	// 親クラスにある obj3dCommon_ を使う
	groundObj->Initialize(obj3dCommon_, modelGround);
	groundObj->SetCamera(camera_);
	object3ds_.push_back(groundObj);

	// スプライト生成
	sprite_ = new Sprite();
	sprite_->Initialize(spriteCommon_);
	sprite_->SetTextureHandle(srvManager_->GetGPUDescriptorHandle(textureResource_.srvIndex));

	// DepthStencil
	depthStencilResource_ = TextureManager::GetInstance()->CreateDepthStencilTextureResource(
		windowProc_->GetClientWidth(), windowProc_->GetClientHeight()
	);
}

void Game::Update(){
	
	Framework::Update();

	// ゲーム固有の更新
	camera_->Update();

	if ( input_->Triggerkey(DIK_SPACE) ) {
		AudioManager::GetInstance()->PlayWave(bgmFile_);
	}
	if ( input_->Triggerkey(DIK_P) ) {
		ParticleManager::GetInstance()->Emit("Circle", { 0.0f, 0.0f, 0.0f }, 10);
	}
	ParticleManager::GetInstance()->Update(camera_);

	for ( Obj3d* obj : object3ds_ ) {
		obj->Update();
	}
	sprite_->Update();

#ifdef USE_IMGUI
	imguiManager_->Begin();
	ImGui::Begin("Debug");
	ImGui::DragFloat3("Pos", &groundPos_.x, 0.1f);
	ImGui::End();
	if ( !object3ds_.empty() ){
		object3ds_[0]->SetTranslation(groundPos_);
	}
#endif
}

void Game::Draw(){

	dxCommon_->PreDraw();
	srvManager_->PreDraw();

	auto commandList = dxCommon_->GetCommandList();

	spriteCommon_->PreDraw(commandList);
	obj3dCommon_->PreDraw(commandList);

	// 描画
	sprite_->Draw();
	for ( Obj3d* obj : object3ds_ ) {
		obj->Draw();
	}

	PipelineManager::GetInstance()->SetPipeline(commandList, PipelineType::Particle);
	ParticleManager::GetInstance()->Draw(commandList);

#ifdef USE_IMGUI
	imguiManager_->End(commandList);
#endif

	dxCommon_->PostDraw();
}

void Game::Finalize(){
	// ゲーム固有の解放
	delete camera_;
	delete sprite_;
	for ( auto obj : object3ds_ ) delete obj;

	textureResource_.resource.Reset();
	depthStencilResource_.Reset();

	Framework::Finalize();
}