#include "Framework.h"

void Framework::Initialize(){
	// ---------------------------------------------
	// 基盤システムの初期化
	// ---------------------------------------------
	// WindowProc
	windowProc_ = new WindowProc();
	WNDCLASS wc = {};
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.hbrBackground = reinterpret_cast< HBRUSH >( COLOR_WINDOW + 1 );
	wc.lpfnWndProc = windowProc_->WndProc;
	windowProc_->Initialize(wc, 1280, 720); // サイズは固定か変数化

	// DirectXCommon
	dxCommon_ = new DirectXCommon();
	dxCommon_->Initialize(windowProc_);

	// Input
	input_ = new Input();
	input_->Initialize(windowProc_->GetHwnd());

	// SrvManager
	srvManager_ = new SrvManager();
	srvManager_->Initialize(dxCommon_);
	dxCommon_->SetSrvManager(srvManager_);

	// ResourceFactory
	resourceFactory_ = new ResourceFactory();
	resourceFactory_->SetDevice(dxCommon_->GetDevice());
	dxCommon_->SetResourceFactory(resourceFactory_);

	// TextureManager
	TextureManager::GetInstance()->Initialize(dxCommon_->GetDevice(), dxCommon_, srvManager_);
	TextureManager::GetInstance()->SetResourceFactory(resourceFactory_);

	// PipelineManager
	PipelineManager::GetInstance()->Initialize(dxCommon_);

	// ImGuiManager
	imguiManager_ = new ImGuiManager();
	imguiManager_->Initialize(windowProc_, dxCommon_);

	// Audio
	AudioManager::GetInstance()->Initialize();

	// 描画共通クラス
	spriteCommon_ = new SpriteCommon();
	spriteCommon_->Initialize(dxCommon_);

	obj3dCommon_ = new Obj3dCommon();
	obj3dCommon_->Initialize(dxCommon_);

	ModelManager::GetInstance()->Initialize(dxCommon_);
	ParticleManager::GetInstance()->Initialize(dxCommon_, srvManager_);

}

void Framework::Finalize(){
	// 各マネージャー終了
	AudioManager::GetInstance()->Finalize();
	ModelManager::GetInstance()->Finalize();
	ParticleManager::GetInstance()->Finalize();
	TextureManager::GetInstance()->Finalize();
	PipelineManager::GetInstance()->Finalize();

	// ImGui終了
	imguiManager_->Shutdown();
	delete imguiManager_;

	// 基盤システム解放
	delete obj3dCommon_;
	delete spriteCommon_;
	delete resourceFactory_;
	delete srvManager_;
	delete input_;
	delete dxCommon_;
	delete windowProc_;
}

void Framework::Update(){
	// 基盤更新
	windowProc_->Update();
	input_->Update();

	// ウィンドウが閉じられたら終了フラグを立てる
	if ( windowProc_->GetIsClosed() ) {
		endRequest_ = true;
	}
}

void Framework::Run(){
	
	// メインループ
	while ( true ) {
		// 毎フレーム更新
		Update();

		// 終了リクエストがあったらループを抜ける
		if ( IsEndRequest() ) {
			break;
		}

		// 描画
		Draw();
	}

}

bool Framework::IsEndRequest(){
	return endRequest_;
}