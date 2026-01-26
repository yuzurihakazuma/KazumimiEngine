#include "SceneManager.h"
#include "IScene.h"
// シングルトンクラスの実装
SceneManager* SceneManager::GetInstance(){
	static SceneManager instance;
	return &instance;
}
// デストラクタ
SceneManager::~SceneManager(){
	if ( currentScene_ ) {
		currentScene_->Finalize();
		delete currentScene_;
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
			// メモリ解放
			delete currentScene_;
		}
		// シーンを切り替え
		currentScene_ = nextScene_;
		// 次のシーン情報をクリア
		nextScene_ = nullptr;
		// 新しいシーンを初期化
		currentScene_->Initialize();
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
// シーン変更
void SceneManager::ChangeScene(IScene* nextScene){
	// 次のシーンをセット
	nextScene_ = nextScene;
}
