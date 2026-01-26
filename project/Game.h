#pragma once
#include "Framework.h" 
// ゲームクラス
class Game : public Framework{
public:
	// オーバーライド 
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	
};