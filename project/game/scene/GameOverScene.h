#pragma once

#include "Engine/Scene/IScene.h"
#include "Engine/Math/Struct.h"

#include <memory>

// ゲームオーバーシーン内で使うクラスを前方宣言する
class Camera;
class MapManager;
class Sprite;

class GameOverScene : public IScene{
public:
    GameOverScene();
    ~GameOverScene();

    void Initialize() override;
    void Update() override;
    void Draw() override;
    void Finalize() override;
    void DrawDebugUI() override{}

private:
    // ブロック背景を映すためのカメラ
    std::unique_ptr<Camera> camera_ = nullptr;

    // タイトルと同じブロック背景を描画するためのマップ
    std::unique_ptr<MapManager> mapManager_ = nullptr;

    // GAME OVERのPNGを描画するスプライト
    std::unique_ptr<Sprite> gameOverSprite_ = nullptr;

	// スペースキーでタイトルに戻ることを促すUIのスプライト
	std::unique_ptr<Sprite> spaceSprite_ = nullptr;

    // タイトル画面と同じ向きで背景を見るためのカメラ設定
    Vector3 gameOverBgCameraPos_ = { 49.0f, 16.0f, 49.1f };
    Vector3 gameOverBgCameraRot_ = { 1.57f, -1.57f, 0.0f };

    // MapManagerの描画基準にする中心位置
    Vector3 gameOverBgPlayerPos_ = { 0.0f, 0.0f, 0.0f };
};