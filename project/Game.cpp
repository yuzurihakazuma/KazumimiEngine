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
	ModelManager::GetInstance()->LoadModel("fence", "resources", "fence.obj");
	ParticleManager::GetInstance()->CreateParticleGroup("Circle", "resources/uvChecker.png");

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
	Obj3d* fence = new Obj3d();
	Model* modelGround = ModelManager::GetInstance()->FindModel("fence");
	// 親クラスにある obj3dCommon_ を使う
	fence->Initialize(obj3dCommon_, modelGround);
	fence->SetCamera(camera_);
	object3ds_.push_back(fence);

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
	
	
	if ( ImGui::TreeNode("Camera") ) {
		// 1. 現在のカメラのTransformを取得（コピーを作成）
		Vector3 camPos = camera_->GetTransform().translate;
		Vector3 camRot = camera_->GetTransform().rotate;

		// 2. ImGuiで数値を操作 (DragFloat3は floatの配列[3] を期待するので、Vector3のアドレスを渡せばOK)
		ImGui::DragFloat3("Position", &camPos.x, 0.1f); // 0.1f刻みで移動
		ImGui::DragFloat3("Rotation", &camRot.x, 0.01f); // 0.01f刻みで回転

		// 3. 変更した値をカメラにセットし直す
		camera_->SetTranslation(camPos);
		camera_->SetRotation(camRot);

		ImGui::TreePop();
	}
	
	ImGui::Text("=== Particle Debug ===");

	size_t particleCount =
		ParticleManager::GetInstance()->GetParticleCount("Circle");
	uint32_t instanceCount =
		ParticleManager::GetInstance()->GetInstanceCount("Circle");

	ImGui::Text("Particles (CPU) : %zu", particleCount);
	ImGui::Text("Instances (GPU) : %u", instanceCount);

	if ( ImGui::Button("Emit 10") ) {
		ParticleManager::GetInstance()->Emit(
			"Circle", { 0.0f, 5.0f, 0.0f }, 10);
	}

	ImGui::Separator();
	ImGui::Text("Press P key to Emit");
	
	
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
	/*sprite_->Draw();
	for ( Obj3d* obj : object3ds_ ) {
		obj->Draw();
	}*/

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