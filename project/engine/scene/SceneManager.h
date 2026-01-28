#pragma once
#include <string>
#include <engine/scene/AbstractSceneFactory.h>
// 前方宣言
class IScene;
// シーンマネージャークラス
class SceneManager{
public:
	// シングルトンインスタンスの取得
	static SceneManager* GetInstance();
	// シーンマネージャーの初期化
	void Update();
	// シーンマネージャーの描画
	void Draw();
	// シーン変更予約
	void SetSceneFactory(AbstractSceneFactory* sceneFactory) { sceneFactory_ = sceneFactory; }
	
	// シーン変更（シーン名で指定）
	void ChangeScene(const std::string& sceneName);

private:
	// コンストラクタを private にして外部からの生成を禁止
	SceneManager() = default;
	~SceneManager();
	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	IScene* currentScene_ = nullptr;
	IScene* nextScene_ = nullptr;

	AbstractSceneFactory* sceneFactory_ = nullptr; 
};
