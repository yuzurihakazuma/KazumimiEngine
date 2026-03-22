#include "Game.h"
// ---  ゲーム固有のファイル ---
#include "scene/SceneFactory.h"      
#include "scene/GamePlayScene.h"     
#include "scene/TitleScene.h"

// ---  エンジン側のファイル ---
#include "engine/scene/SceneManager.h"   
#include "engine/base/DirectXCommon.h"   
#include "engine/graphics/SrvManager.h"
#include "engine/utils/ImGuiManager.h"

void Game::Initialize(){
	// 基盤システムの初期化 (Window, DirectX, Input, Common類)
	Framework::Initialize();

	// 1. ファクトリーの生成
	sceneFactory_ = std::make_unique<SceneFactory>();
	// 2. マネージャーにファクトリーを教える
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());

	// 3. 最初のシーンを文字列でリクエストする
	SceneManager::GetInstance()->SetFirstScene(std::make_unique<GamePlayScene>());


}

void Game::Update(){
	// 基盤更新
	Framework::Update();

#ifdef USE_IMGUI

	// ImGuiのフレーム開始処理
	ImGuiManager::GetInstance()->Begin();
#endif
	// シーンの更新
	SceneManager::GetInstance()->Update();

}

void Game::Draw(){
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	// 描画前処理
	dxCommon->PreDraw();
	// SRVマネージャーの描画前処理
	SrvManager::GetInstance()->PreDraw();

	// シーンの描画
	SceneManager::GetInstance()->Draw();

#ifdef USE_IMGUI

	// ImGuiの描画コマンド発行
	ImGuiManager::GetInstance()->End(dxCommon->GetCommandList());
#endif
	// 描画後処理
	dxCommon->PostDraw();
}

Game::Game(){}

void Game::Finalize(){
	// 基盤終了
	Framework::Finalize();
}