#include "Game.h"
#include <engine/scene/GamePlayScene.h>
#include <engine//scene/SceneManager.h>
#include <engine/scene/SceneFactory.h>
void Game::Initialize(){
	// 基盤システムの初期化 (Window, DirectX, Input, Common類)
	Framework::Initialize();

	// 1. ファクトリーの生成
	sceneFactory_ = new SceneFactory();

	// 2. マネージャーにファクトリーを教える
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_);

	// 3. 最初のシーンを文字列でリクエストする
	SceneManager::GetInstance()->ChangeScene("TITLE");


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
	// シーンマネージャーの終了
	delete sceneFactory_;
	// 基盤終了
	Framework::Finalize();
}