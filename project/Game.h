#pragma once
#include "Framework.h" 
#include <memory>

class SceneFactory;
// ゲームクラス
class Game : public Framework{
public:
	// オーバーライド 
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;


	Game();

	~Game() = default;

private:

	std::unique_ptr<SceneFactory> sceneFactory_ = nullptr;

};