#include "Game.h"
// ---  ゲーム固有のファイル ---
#include "SceneFactory.h"
#include "GamePlayScene.h"
#include "TitleScene.h"

// ---  エンジン側のファイル ---
#include "engine/scene/SceneManager.h"
#include "engine/base/DirectXCommon.h"
#include "game/transition/KamishibaiTransition.h"
#include "engine/utils/EditorManager.h"

#include <Windows.h>

void Game::Initialize(){
	// 基盤システムの初期化 (Window, DirectX, Input, Common類, EditorManager)
	Framework::Initialize();

	// 1. ファクトリーの生成
	sceneFactory_ = std::make_unique<SceneFactory>();

	// 2. マネージャーにファクトリーを教える
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());

	// 2.5 紙芝居の遷移（シーンが変わっても残る舞台）。画像・音は起動時に読む
	DirectXCommon::GetInstance()->BeginCommandRecording();
	KamishibaiTransition::GetInstance()->Load();
	DirectXCommon::GetInstance()->EndCommandRecording();
	SceneManager::GetInstance()->SetTransition(KamishibaiTransition::GetInstance());

	// 3. 最初のシーンをリクエストする
	//    提出用にステージ選択から始める（タイトルから始める時は kDefaultStartScene を "TITLE" にする）
	//    確認用：環境変数 CG2_START_SCENE にシーン名（TITLE / STAGESELECT / GAMEPLAY）を入れて起動すると、そのシーンから始まる
	constexpr const char* kDefaultStartScene = "STAGESELECT";
	char startSceneName[32] = {};
	std::unique_ptr<IScene> startScene;
	if ( GetEnvironmentVariableA("CG2_START_SCENE", startSceneName, sizeof(startSceneName)) > 0 ) {
		startScene = sceneFactory_->CreateScene(startSceneName);
	}
	if ( !startScene ) { startScene = sceneFactory_->CreateScene(kDefaultStartScene); }
	if ( !startScene ) { startScene = std::make_unique<TitleScene>(); }
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
