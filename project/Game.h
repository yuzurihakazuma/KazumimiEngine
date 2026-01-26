#pragma once
#include "Framework.h" 
#include "GamePlayScene.h" 
// ゲームクラス
class Game : public Framework{
public:
	// オーバーライド 
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	// シーンの実体
	GamePlayScene* scene_ = nullptr;
};