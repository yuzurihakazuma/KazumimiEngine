#include "Game.h"
// ---  ゲーム固有のファイル ---
#include "SceneFactory.h"      
#include "GamePlayScene.h"     
#include "TitleScene.h"

// ---  エンジン側のファイル ---
#include "engine/scene/SceneManager.h"   
#include "engine/base/DirectXCommon.h"   
#include "engine/graphics/SrvManager.h"
#include "engine/utils/EditorManager.h"

void Game::Initialize(){
	// 基盤システムの初期化 (Window, DirectX, Input, Common類)
	Framework::Initialize();

	// エディタマネージャーの生成
	editorManager_ = std::make_unique<EditorManager>();


	// 1. ファクトリーの生成
	sceneFactory_ = std::make_unique<SceneFactory>();

	

	// 2. マネージャーにファクトリーを教える
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());

	// 3. 最初のシーンを文字列でリクエストする
	SceneManager::GetInstance()->ChangeScene(std::make_unique<GamePlayScene>());


}

void Game::Update(){
	// 基盤更新
	Framework::Update();

	// エディタ更新
	editorManager_->Begin();


	// シーンの更新
	SceneManager::GetInstance()->Update();

	// パフォーマンス計測値をエディタに渡す
	editorManager_->SetCpuTimes(
		SceneManager::GetInstance()->GetCpuUpdateTimeMs(),
		SceneManager::GetInstance()->GetCpuDrawTimeMs()
	);

	// エディタUIの更新・描画
	editorManager_->Update();


}

void Game::Draw(){
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();

	// 描画前処理
	dxCommon->PreDraw();
	// SRVマネージャーの描画前処理
	SrvManager::GetInstance()->PreDraw();

	
	// シーンの描画
	SceneManager::GetInstance()->Draw();

	// エディタの描画前処理
	editorManager_->End();


	// 描画後処理
	dxCommon->PostDraw();
}

Game::Game(){}

Game::~Game() = default;


void Game::Finalize(){
	// 基盤終了
	Framework::Finalize();
}