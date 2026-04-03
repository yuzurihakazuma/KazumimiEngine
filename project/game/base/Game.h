#pragma once
#include "engine/Base/Framework.h"
#include <memory>

class SceneFactory;
class EditorManager;

// ゲームクラス
class Game : public Framework{
public:
	// オーバーライド 
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;


public: // コンストラクタ・デストラクタ

	Game();

	~Game();

private:
	// シーンファクトリー
	std::unique_ptr<SceneFactory> sceneFactory_ = nullptr;
	std::unique_ptr<EditorManager> editorManager_ = nullptr;


};