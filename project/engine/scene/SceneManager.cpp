#include "SceneManager.h"
// --- 標準ライブラリ ---
#include <cassert>

// --- エンジン側のファイル ---
#include "engine/scene/IScene.h"
#include "engine/scene/AbstractSceneFactory.h"
#include "engine/base/DirectXCommon.h"

// シングルトンクラスの実装
SceneManager* SceneManager::GetInstance(){
	static SceneManager instance;
	return &instance;
}
// デストラクタ
SceneManager::~SceneManager(){
	// 現在のシーンを終了
	if ( currentScene_ ) {
		// 現在のシーンを終了
		currentScene_->Finalize();
		// シーン情報をクリア
		currentScene_ = nullptr;
	}
}
// シーンマネージャーの更新
void SceneManager::Update(){
	// 次のシーンへの切り替え処理
	if ( nextScene_ ) {
		// 現在のシーンを終了
		if ( currentScene_ ) {
			currentScene_->Finalize();
		}
		// シーンを切り替え
		currentScene_ = std::move(nextScene_);
		// 次のシーン情報をクリア
		nextScene_ = nullptr;
		DirectXCommon* dxCommon = DirectXCommon::GetInstance();

		dxCommon->BeginCommandRecording();

		currentScene_->Initialize();
		dxCommon->EndCommandRecording();
	}
	// 現在のシーンを更新
	if ( currentScene_ ) {
		currentScene_->Update();
	}

	

}
// シーンマネージャーの描画
void SceneManager::Draw(){
	if ( currentScene_ ) {
		// 現在のシーンを描画
		currentScene_->Draw();
	}
}

// 文字列を使ってファクトリーにシーンを作ってもらう
void SceneManager::ChangeScene(const std::string& sceneName) {
	assert(sceneFactory_ && "SceneManager: SceneFactory is not set!");

	// ファクトリーが unique_ptr を作ってくれるので、そのまま受け取る
	nextScene_ = sceneFactory_->CreateScene(sceneName);
}

// シーン変更（シーン名で指定）
void SceneManager::ChangeScene(std::unique_ptr<IScene> nextScene){
	nextScene_ = std::move(nextScene);
}
