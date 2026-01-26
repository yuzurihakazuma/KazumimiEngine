#include "Game.h"

void Game::Initialize(){
	// 基盤システムの初期化 (Window, DirectX, Input, Common類)
	Framework::Initialize();

	// シーン生成
	scene_ = new GamePlayScene();

	// シーン初期化
	scene_->Initialize();
}

void Game::Update(){
	// 基盤更新
	Framework::Update();

#ifdef USE_IMGUI
	// ImGuiのフレーム開始処理
	imguiManager_->Begin();
#endif

	// シーンの更新
	scene_->Update();

}

void Game::Draw(){
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	
	// 描画前処理
	dxCommon->PreDraw();
	// SRVマネージャーの描画前処理
	srvManager_->PreDraw();

	// シーンの描画
	scene_->Draw();

#ifdef USE_IMGUI
	// ImGuiの描画コマンド発行
	imguiManager_->End(dxCommon->GetCommandList());
#endif

	// 描画後処理
	dxCommon->PostDraw();
}

void Game::Finalize(){
	// シーン終了
	if ( scene_ ) {
		scene_->Finalize();
		delete scene_;
	}

	// 基盤終了
	Framework::Finalize();
}