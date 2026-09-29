#pragma once
// =====================================================================
//  TitleScene：タイトルシーン。
//   SDFのタイトルロゴ（拡大しても滲まない。グローと点滅はパラメータ制御）とスタート案内を出し、
//   T でゲームプレイシーンへ進む。マップエディタもここから触れる
// =====================================================================
#include "engine/scene/IScene.h"
#include "engine/graphics/TextureManager.h"
#include "engine/math/struct.h"

#include <memory>
#include <string>
#include <unordered_map>

class DebugCamera;
class Camera;
class Sprite;
class LevelEditor;
class SDFSprite;
class SDFText;

class TitleScene : public IScene{
public:
	TitleScene();
	~TitleScene();

	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;
	void Reload() override;
	void DrawDebugUI() override;

private:
	// タイトルロゴの脈動・スタート案内の点滅
	void UpdateTitleAnimation();

	std::unique_ptr<Camera>      camera_;
	std::unique_ptr<DebugCamera> debugCamera_;

	// uvChecker のスプライト（位置はデバッグUIで調整できる）
	std::unique_ptr<Sprite> sprite_;
	Vector2 spritePos_ = { 100.0f, 100.0f };

	std::unordered_map<std::string, TextureData> textures_;
	std::string bgmFile_ = "resources/BGMDon.mp3";

	// マップエディタ
	std::unique_ptr<LevelEditor> levelEditor_;

	// SDFタイトルロゴ＋スタート案内
	std::unique_ptr<SDFSprite> logoSprite_;
	std::unique_ptr<SDFText>   startText_;
	float titleAnimTime_ = 0.0f;
};
