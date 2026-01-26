#pragma once
#include "Framework.h" 
#include <engine/scene/GamePlayScene.h>
#include <engine/scene/TitleScene.h>
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
	TitleScene* scene_ = nullptr;


};