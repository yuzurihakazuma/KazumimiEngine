#include "Game.h"
#include <engine/scene/GamePlayScene.h>
#include <engine/scene/TitleScene.h>
#include <engine//scene/SceneManager.h>
void Game::Initialize(){
	// 基盤システムの初期化 (Window, DirectX, Input, Common類)
	Framework::Initialize();

	// シーン生成
	SceneManager* scene = SceneManager::GetInstance();
	
	// 最初のシーンをタイトルシーンに設定
	scene->SetChangeScene(new TitleScene());



}

void Game::Update(){
	// 基盤更新
	Framework::Update();

#ifdef USE_IMGUI
	// ImGuiのフレーム開始処理
	imguiManager_->Begin();
#endif

	// シーンの更新
	SceneManager::GetInstance()->Update();

}

void Game::Draw(){
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	// 描画前処理
	dxCommon->PreDraw();
	// SRVマネージャーの描画前処理
	srvManager_->PreDraw();

	// シーンの描画
	SceneManager::GetInstance()->Draw();

#ifdef USE_IMGUI
	// ImGuiの描画コマンド発行
	imguiManager_->End(dxCommon->GetCommandList());
#endif

	// 描画後処理
	dxCommon->PostDraw();
}

void Game::Finalize(){
	// 基盤終了
	Framework::Finalize();
}