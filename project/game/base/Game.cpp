#include "Game.h"
// ---  ゲーム固有のファイル ---
#include "SceneFactory.h"
#include "GamePlayScene.h"

// ---  エンジン側のファイル ---
#include "engine/scene/SceneManager.h"
#include "engine/utils/EditorManager.h"

#include <Windows.h>

void Game::Initialize(){
	// 基盤システムの初期化 (Window, DirectX, Input, Common類, EditorManager)
	Framework::Initialize();

	// 1. ファクトリーの生成
	sceneFactory_ = std::make_unique<SceneFactory>();

	// 2. マネージャーにファクトリーを教える
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());

	// 3. 最初のシーンをリクエストする
	//    確認用：環境変数 CG2_START_SCENE にシーン名（TITLE / GAMEPLAY）を入れて起動すると、そのシーンから始まる
	char startSceneName[32] = {};
	std::unique_ptr<IScene> startScene;
	if ( GetEnvironmentVariableA("CG2_START_SCENE", startSceneName, sizeof(startSceneName)) > 0 ) {
		startScene = sceneFactory_->CreateScene(startSceneName);
	}
	if ( !startScene ) { startScene = std::make_unique<GamePlayScene>(); }
	SceneManager::GetInstance()->ChangeScene(std::move(startScene));
}

void Game::Update(){
	// 基盤更新（入力・ウィンドウ・リサイズ・ImGui フレーム開始）
	Framework::Update();

	// シーンの更新
	SceneManager::GetInstance()->Update();

	// パフォーマンス計測値をエディタに渡す
	EditorManager::GetInstance()->SetCpuTimes(
		SceneManager::GetInstance()->GetCpuUpdateTimeMs(),
		SceneManager::GetInstance()->GetCpuDrawTimeMs()
	);

	// エディタ UI の更新
	EditorManager::GetInstance()->Update();
}

void Game::DrawScene(){
	// シーン固有の描画（PreDraw / PostDraw / ImGui End は Framework::Draw() が管理する）
	SceneManager::GetInstance()->Draw();
}

Game::Game(){}

void Game::Finalize(){
	// 基盤終了（EditorManager の終了処理も Framework::Finalize() が行う）
	Framework::Finalize();
}
