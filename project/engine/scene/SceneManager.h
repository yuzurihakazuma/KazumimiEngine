#pragma once
// --- 標準ライブラリ ---
#include <memory>
#include <string>

// 前方宣言
class IScene;
class AbstractSceneFactory;

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
	
	void ChangeScene(const std::string& sceneName);

	// シーン変更（シーン名で指定）
	void ChangeScene(std::unique_ptr<IScene> nextScene);

	void Finalize();

	// エディタのアクティブ状態を管理
	bool IsEditorActive() const{ return isEditorActive_; }


private:
	// コンストラクタを private にして外部からの生成を禁止
	SceneManager() = default;
	~SceneManager();
	SceneManager(const SceneManager&) = delete;
	SceneManager& operator=(const SceneManager&) = delete;

	std::unique_ptr<IScene> currentScene_ = nullptr;
	std::unique_ptr<IScene> nextScene_ = nullptr;

	AbstractSceneFactory* sceneFactory_ = nullptr; 

	bool isEditorActive_ = false;

};
