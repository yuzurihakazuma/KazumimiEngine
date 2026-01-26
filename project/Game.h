#pragma once
#include "Framework.h" 
#include "scene/GamePlayScene.h"
#include""
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